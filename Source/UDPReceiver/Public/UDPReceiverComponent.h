// UDPReceiverComponent.h
// Component to receive UDP data from ESP32 (rotation angle)
// Supports automatic ESP32 discovery

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/UdpSocketReceiver.h"
#include "Common/UdpSocketBuilder.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "UDPReceiverComponent.generated.h"

// Delegate to notify when a new angle arrives
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAngleReceived, float, Angle);

// Delegate to notify when ESP32 is found/lost
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnESP32ConnectionChanged, bool, bConnected);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UDPRECEIVER_API UUDPReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UUDPReceiverComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============== UDP CONFIGURATION (Editable in Editor) ==============
	
	/** UDP port to receive angle data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	int32 DataPort = 5005;

	/** UDP port for discovery broadcast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	int32 DiscoveryPort = 5006;

	/** IP to bind to (0.0.0.0 = all interfaces) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	FString ListenIP = TEXT("0.0.0.0");

	/** Broadcast address for discovery (255.255.255.255 = global broadcast) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	FString BroadcastIP = TEXT("255.255.255.255");

	/** Discovery send interval in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float DiscoveryIntervalSeconds = 2.0f;

	/** Timeout to consider ESP32 disconnected (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float ConnectionTimeoutSeconds = 5.0f;

	// ============== ROTATION CONFIGURATION ==============

	/** Rotation axis to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Config")
	TEnumAsByte<EAxis::Type> RotationAxis = EAxis::Z;

	/** Angle multiplier (e.g. -1 to invert direction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Config")
	float AngleMultiplier = 1.0f;

	/** Additional offset to the received angle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Config")
	float AngleOffset = 0.0f;

	/** If true, applies smoothing to rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Config")
	bool bEnableSmoothing = true;

	/** Interpolation speed (higher = more responsive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Config", meta = (EditCondition = "bEnableSmoothing", ClampMin = "1.0", ClampMax = "50.0"))
	float SmoothingSpeed = 15.0f;

	/** If true, applies rotation automatically to the owner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Config")
	bool bAutoApplyRotation = true;

	// ============== RUNTIME VALUES (Read-only) ==============
	
	/** Last raw angle received from ESP32 */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Status")
	float RawAngle = 0.0f;

	/** Processed angle (with multiplier and offset) */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Status")
	float ProcessedAngle = 0.0f;

	/** Current angle after smoothing */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Status")
	float SmoothedAngle = 0.0f;

	/** True if socket is connected and listening */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Status")
	bool bIsListening = false;

	/** True if ESP32 has been found and is sending data */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Status")
	bool bESP32Connected = false;

	/** IP of connected ESP32 */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Status")
	FString ESP32Address = TEXT("");

	/** Received packet counter */
	UPROPERTY(BlueprintReadOnly, Category = "UDP Status")
	int32 PacketsReceived = 0;

	// ============== EVENTS (Blueprint assignable) ==============
	
	/** Event called every time a new angle arrives */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FOnAngleReceived OnAngleReceived;

	/** Event called when ESP32 connects or disconnects */
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FOnESP32ConnectionChanged OnESP32ConnectionChanged;

	// ============== BLUEPRINT FUNCTIONS ==============
	
	/** Starts UDP reception and discovery */
	UFUNCTION(BlueprintCallable, Category = "UDP")
	bool StartListening();

	/** Stops UDP reception */
	UFUNCTION(BlueprintCallable, Category = "UDP")
	void StopListening();

	/** Resets angle to zero */
	UFUNCTION(BlueprintCallable, Category = "UDP")
	void ResetAngle();

	/** Manually sends a discovery (normally automatic) */
	UFUNCTION(BlueprintCallable, Category = "UDP")
	void SendDiscovery();

private:
	// Socket to receive data
	FSocket* DataSocket = nullptr;
	
	// Socket to send discovery
	FSocket* DiscoverySocket = nullptr;
	
	// Asynchronous receiver for data
	FUdpSocketReceiver* UDPReceiver = nullptr;

	// Thread-safe: angle received from network thread
	TAtomic<float> ThreadSafeAngle;

	// Flag to indicate new data is available
	TAtomic<bool> bHasNewData;

	// Timing
	float TimeSinceLastDiscovery = 0.0f;
	float TimeSinceLastPacket = 0.0f;

	// Callback called when data arrives
	void OnUDPDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

	// Applies rotation to the owner
	void ApplyRotation(float Angle);

	// Angular interpolation that handles 0-360 wrap-around
	float LerpAngle(float Current, float Target, float Alpha);

	// Updates connection status
	void UpdateConnectionStatus(bool bConnected, const FString& Address = TEXT(""));
};
