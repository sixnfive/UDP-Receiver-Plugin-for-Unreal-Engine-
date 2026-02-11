// UDPReceiverComponent.cpp
// Implementation of the component for UDP reception with automatic discovery

#include "UDPReceiverComponent.h"
#include "Async/Async.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"

UUDPReceiverComponent::UUDPReceiverComponent()
{
	// Enable Tick to update rotation and send discovery
	PrimaryComponentTick.bCanEverTick = true;
    
	// Initialize atomics
	ThreadSafeAngle = 0.0f;
	bHasNewData = false;
}

void UUDPReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
    
	UE_LOG(LogTemp, Warning, TEXT("=== UDP RECEIVER COMPONENT BEGIN PLAY ==="));
    
	// Automatically start listening
	if (StartListening())
	{
		UE_LOG(LogTemp, Warning, TEXT("UDP Receiver: StartListening SUCCESS"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UDP Receiver: StartListening FAILED"));
	}
}

void UUDPReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop listening and clean up resources
	StopListening();
    
	Super::EndPlay(EndPlayReason);
}

bool UUDPReceiverComponent::StartListening()
{
	// If already listening, do nothing
	if (bIsListening)
	{
		UE_LOG(LogTemp, Warning, TEXT("UDP Receiver: Already listening"));
		return true;
	}

	// Get the socket subsystem
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP Receiver: Failed to get socket subsystem"));
		return false;
	}

	// Create the local endpoint for data
	FIPv4Address ParsedIP;
	if (!FIPv4Address::Parse(ListenIP, ParsedIP))
	{
		UE_LOG(LogTemp, Error, TEXT("UDP Receiver: Invalid IP address: %s"), *ListenIP);
		return false;
	}
    
	FIPv4Endpoint DataEndpoint(ParsedIP, DataPort);

	// ===== SOCKET TO RECEIVE DATA (port 5005) =====
	DataSocket = FUdpSocketBuilder(TEXT("UDP_Data_Socket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(DataEndpoint)
		.WithReceiveBufferSize(2 * 1024 * 1024)
		.Build();

	if (!DataSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP Receiver: Failed to create data socket on port %d"), DataPort);
		return false;
	}

	// ===== SOCKET TO SEND DISCOVERY (port 5006) =====
	DiscoverySocket = FUdpSocketBuilder(TEXT("UDP_Discovery_Socket"))
		.AsNonBlocking()
		.AsReusable()
		.WithBroadcast()  // Enable broadcast
		.Build();

	if (!DiscoverySocket)
	{
		UE_LOG(LogTemp, Error, TEXT("UDP Receiver: Failed to create discovery socket"));
		// Clean up the data socket
		DataSocket->Close();
		SocketSubsystem->DestroySocket(DataSocket);
		DataSocket = nullptr;
		return false;
	}

	// Create the asynchronous receiver for data
	FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);
	UDPReceiver = new FUdpSocketReceiver(
		DataSocket,
		ThreadWaitTime,
		TEXT("UDP_Data_Receiver_Thread")
	);
    
	// Bind to the callback
	UDPReceiver->OnDataReceived().BindUObject(this, &UUDPReceiverComponent::OnUDPDataReceived);
    
	// Start the receiving thread
	UDPReceiver->Start();

	bIsListening = true;
    
	UE_LOG(LogTemp, Log, TEXT("UDP Receiver: Started listening on port %d, discovery on port %d"), DataPort, DiscoveryPort);
    
	// Immediately send the first discovery
	SendDiscovery();
    
	return true;
}

void UUDPReceiverComponent::StopListening()
{
	// Stop the receiver
	if (UDPReceiver)
	{
		UDPReceiver->Stop();
		delete UDPReceiver;
		UDPReceiver = nullptr;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// Close and destroy the data socket
	if (DataSocket)
	{
		DataSocket->Close();
		if (SocketSubsystem) SocketSubsystem->DestroySocket(DataSocket);
		DataSocket = nullptr;
	}

	// Close and destroy the discovery socket
	if (DiscoverySocket)
	{
		DiscoverySocket->Close();
		if (SocketSubsystem) SocketSubsystem->DestroySocket(DiscoverySocket);
		DiscoverySocket = nullptr;
	}

	bIsListening = false;
	UpdateConnectionStatus(false);
    
	UE_LOG(LogTemp, Log, TEXT("UDP Receiver: Stopped listening"));
}

void UUDPReceiverComponent::ResetAngle()
{
	RawAngle = 0.0f;
	ProcessedAngle = 0.0f;
	SmoothedAngle = 0.0f;
	ThreadSafeAngle = 0.0f;
}

void UUDPReceiverComponent::SendDiscovery()
{
	if (!DiscoverySocket)
	{
		return;
	}

	// Prepare the broadcast endpoint
	FIPv4Address BroadcastAddress;
	if (!FIPv4Address::Parse(BroadcastIP, BroadcastAddress))
	{
		UE_LOG(LogTemp, Warning, TEXT("UDP Discovery: Invalid broadcast IP: %s"), *BroadcastIP);
		return;
	}
    
	FIPv4Endpoint BroadcastEndpoint(BroadcastAddress, DiscoveryPort);
	TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	RemoteAddr->SetIp(BroadcastAddress.Value);
	RemoteAddr->SetPort(DiscoveryPort);

	// Discovery message (must match what the ESP32 expects)
	const char* DiscoveryMessage = "DISCOVER";
	int32 BytesSent = 0;
    
	bool bSuccess = DiscoverySocket->SendTo(
		(const uint8*)DiscoveryMessage,
		FCStringAnsi::Strlen(DiscoveryMessage),
		BytesSent,
		*RemoteAddr
	);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Verbose, TEXT("UDP Discovery: Sent broadcast to %s:%d"), *BroadcastIP, DiscoveryPort);
	}
}

void UUDPReceiverComponent::OnUDPDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
	// This function is called from a network thread!
	// Do not access UE objects directly here
    
	// Verify that the data has the correct size (float = 4 bytes)
	if (Data->Num() >= sizeof(float))
	{
		float ReceivedAngle;
        
		// Read the float from the bytes (little-endian, like the ESP32)
		FMemory::Memcpy(&ReceivedAngle, Data->GetData(), sizeof(float));
        
		// Validate the angle (should be 0-360)
		if (ReceivedAngle >= 0.0f && ReceivedAngle <= 360.0f)
		{
			// Save in a thread-safe way
			ThreadSafeAngle = ReceivedAngle;
			bHasNewData = true;
		}
	}
}

void UUDPReceiverComponent::UpdateConnectionStatus(bool bConnected, const FString& Address)
{
	if (bESP32Connected != bConnected)
	{
		bESP32Connected = bConnected;
		ESP32Address = Address;
        
		if (bConnected)
		{
			UE_LOG(LogTemp, Log, TEXT("UDP Receiver: ESP32 connected from %s"), *Address);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UDP Receiver: ESP32 disconnected"));
		}
        
		// Broadcast event
		OnESP32ConnectionChanged.Broadcast(bConnected);
	}
}

void UUDPReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ===== PERIODIC DISCOVERY =====
	TimeSinceLastDiscovery += DeltaTime;
	if (TimeSinceLastDiscovery >= DiscoveryIntervalSeconds)
	{
		TimeSinceLastDiscovery = 0.0f;
		SendDiscovery();
	}

	// ===== HANDLING RECEIVED DATA =====
	if (bHasNewData.Exchange(false))
	{
		// Read the angle in a thread-safe way
		RawAngle = ThreadSafeAngle.Load();
		PacketsReceived++;
        
		// Reset the timeout timer
		TimeSinceLastPacket = 0.0f;
        
		// Update connection status (connected)
		if (!bESP32Connected)
		{
			UpdateConnectionStatus(true, TEXT("Detected"));
		}
        
		// Apply multiplier and offset
		ProcessedAngle = (RawAngle * AngleMultiplier) + AngleOffset;
        
		// Normalize to 0-360
		ProcessedAngle = FMath::Fmod(ProcessedAngle + 360.0f, 360.0f);
        
		// Broadcast the event
		OnAngleReceived.Broadcast(ProcessedAngle);
	}
	else
	{
		// No data received, increment timeout timer
		TimeSinceLastPacket += DeltaTime;
        
		// Check connection timeout
		if (bESP32Connected && TimeSinceLastPacket > ConnectionTimeoutSeconds)
		{
			UpdateConnectionStatus(false);
		}
	}

	// ===== SMOOTHING =====
	if (bEnableSmoothing)
	{
		SmoothedAngle = LerpAngle(SmoothedAngle, ProcessedAngle, DeltaTime * SmoothingSpeed);
	}
	else
	{
		SmoothedAngle = ProcessedAngle;
	}

	// ===== APPLY ROTATION =====
	if (bAutoApplyRotation)
	{
		ApplyRotation(SmoothedAngle);
	}
}

float UUDPReceiverComponent::LerpAngle(float Current, float Target, float Alpha)
{
	// Correctly handles 0-360 wrap-around
	// Ex: from 350° to 10° should go forward, not backward by 340°
    
	float Difference = Target - Current;
    
	// Normalize the difference to -180...180
	while (Difference > 180.0f) Difference -= 360.0f;
	while (Difference < -180.0f) Difference += 360.0f;
    
	float Result = Current + Difference * FMath::Clamp(Alpha, 0.0f, 1.0f);
    
	// Normalize the result to 0-360
	while (Result < 0.0f) Result += 360.0f;
	while (Result >= 360.0f) Result -= 360.0f;
    
	return Result;
}

void UUDPReceiverComponent::ApplyRotation(float Angle)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Get the current rotation
	FRotator CurrentRotation = Owner->GetActorRotation();
    
	// Apply the angle to the specified axis
	switch (RotationAxis)
	{
		case EAxis::X:
			CurrentRotation.Roll = Angle;
			break;
		case EAxis::Y:
			CurrentRotation.Pitch = Angle;
			break;
		case EAxis::Z:
		default:
			CurrentRotation.Yaw = Angle;
			break;
	}
    
	// Set the new rotation
	Owner->SetActorRotation(CurrentRotation);
}
