# UDP Receiver Plugin for Unreal Engine

Plugin to receive rotation data via UDP from ESP32 or other network devices.

## Features

- **Asynchronous UDP reception** - Receives data without blocking the game thread
- **Automatic discovery** - Automatically finds ESP32 devices on the network
- **Rotation smoothing** - Smooth interpolation of angles
- **Thread-safe** - Safe handling of network data
- **Blueprint configurable** - All settings accessible from the editor
- **Blueprint events** - React to received data with custom events

## Installation

1. Copy the entire `UDPReceiver` folder to `YourProject/Plugins/`
2. Restart Unreal Engine
3. The plugin will be automatically compiled on first launch

## Basic Usage

### 1. Add the component to an Actor

1. Select an Actor in the scene
2. In the Details panel, click "Add Component"
3. Search for "UDP Receiver Component"
4. Add the component

### 2. Configure settings

**UDP Parameters:**
- `DataPort` (default: 5005) - Port to receive angles
- `DiscoveryPort` (default: 5006) - Port for discovery broadcast
- `ListenIP` (default: 0.0.0.0) - IP to bind to
- `BroadcastIP` (default: 255.255.255.255) - Broadcast address
- `DiscoveryIntervalSeconds` (default: 2.0) - Discovery send interval
- `ConnectionTimeoutSeconds` (default: 5.0) - Disconnection timeout

**Rotation Parameters:**
- `RotationAxis` (default: Z) - Rotation axis (X=Roll, Y=Pitch, Z=Yaw)
- `AngleMultiplier` (default: 1.0) - Multiplier (-1 to invert)
- `AngleOffset` (default: 0.0) - Offset in degrees
- `bEnableSmoothing` (default: true) - Enable smoothing
- `SmoothingSpeed` (default: 15.0) - Interpolation speed
- `bAutoApplyRotation` (default: true) - Automatically apply rotation

### 3. Test with Python ESP32 simulator

The plugin includes a full-featured ESP32 simulator (`esp32_simulator.py`) with multiple simulation modes:

**Basic Usage:**
```bash
python3 esp32_simulator.py --target-ip 127.0.0.1
```

**Advanced Options:**
```bash
# Continuous rotation at 30 degrees/second (default)
python3 esp32_simulator.py --target-ip 127.0.0.1 --mode rotate --speed 30.0

# Sine wave oscillation
python3 esp32_simulator.py --target-ip 127.0.0.1 --mode sine

# Static angle (45 degrees)
python3 esp32_simulator.py --target-ip 127.0.0.1 --mode static

# Random walk
python3 esp32_simulator.py --target-ip 127.0.0.1 --mode random

# Custom send rate (default is 100 Hz)
python3 esp32_simulator.py --target-ip 127.0.0.1 --rate 50
```

**Command Line Arguments:**
- `--target-ip`: Target PC IP address (default: 127.0.0.1)
- `--data-port`: Data port (default: 5005)
- `--discovery-port`: Discovery port (default: 5006)
- `--rate`: Send rate in Hz (default: 100)
- `--mode`: Simulation mode: `rotate`, `sine`, `static`, `random`
- `--speed`: Rotation speed in degrees/sec (for rotate mode, default: 30.0)

**Expected Output**

The simulator will display:
```
============================================================
ESP32 SIMULATOR
============================================================
Target IP:       127.0.0.1
Data Port:       5005
Discovery Port:  5006
Send Rate:       100.0 Hz
Mode:            rotate
Rotation Speed:  30.0 deg/sec
============================================================

Press Ctrl+C to stop

Angle:  45.00° | Packets:   100 | Rate:  100.0 Hz
Angle:  90.00° | Packets:   200 | Rate:  100.0 Hz
```

## 📘 Blueprint API

### Functions

- `StartListening()` → bool - Start UDP reception
- `StopListening()` - Stop UDP reception
- `ResetAngle()` - Reset angle to zero
- `SendDiscovery()` - Manually send a discovery broadcast

### Events

- `OnAngleReceived(float Angle)` - Called when a new angle arrives
- `OnESP32ConnectionChanged(bool bConnected)` - Called when ESP32 connects/disconnects

### State Variables (Read-Only)

- `RawAngle` - Raw received angle (0-360°)
- `ProcessedAngle` - Processed angle (with multiplier and offset)
- `SmoothedAngle` - Final angle after smoothing
- `bIsListening` - True if socket is active
- `bESP32Connected` - True if ESP32 is sending data
- `ESP32Address` - IP of connected device
- `PacketsReceived` - Received packet counter

## Blueprint Example

```
Event OnAngleReceived
│
├─ Print String: "Angle received: {Angle}"
│
└─ Set Actor Rotation
   └─ Make Rotator (0, 0, Angle)
```

## Communication Protocol

### Data Format
- **Type**: Float (4 bytes, little-endian)
- **Range**: 0.0 - 360.0 degrees
- **Port**: 5005 (configurable)

### Discovery Protocol
- **Message**: "DISCOVER" (ASCII string)
- **Port**: 5006 (configurable)
- **Direction**: Unreal → Broadcast
- **Interval**: 2 seconds (configurable)

## Requirements

- **Unreal Engine**: 5.4 - 5.6+ (tested on 5.4, 5.5, 5.6)
- **Platforms**: Windows, Mac, Linux
- **Modules**: Sockets, Networking (included automatically)


##  Troubleshooting

### Component doesn't appear in the list

1. Verify the plugin is in `Plugins/UDPReceiver/`
2. Restart Unreal Engine
3. Check that the plugin is enabled in Edit → Plugins

### Compilation errors

1. Verify that Unreal Engine version is 5.4+
2. Delete the `Binaries/` and `Intermediate/` folders of the plugin
3. Regenerate project files
4. Recompile

### Not receiving UDP data

1. Verify that ports 5005 and 5006 are open in the firewall
2. Check that the Python simulator is sending to the correct IP
3. Check the Output Log for error messages
4. Verify that `bIsListening` is true

### ESP32 disconnects continuously

1. Increase `ConnectionTimeoutSeconds` (e.g. 10.0)
2. Verify network connection stability
3. Check that ESP32 is sending data regularly


