#!/usr/bin/env python3
"""
ESP32 Simulator - Sends UDP data to Unreal Engine
Simulates the AS5600 encoder behavior
"""

import socket
import struct
import time
import argparse
import math

def main():
    parser = argparse.ArgumentParser(description='Simulate ESP32 sending angle data to Unreal')
    parser.add_argument('--target-ip', type=str, default='127.0.0.1', 
                        help='Target PC IP address (default: localhost)')
    parser.add_argument('--data-port', type=int, default=5005, 
                        help='Data port (default: 5005)')
    parser.add_argument('--discovery-port', type=int, default=5006, 
                        help='Discovery port (default: 5006)')
    parser.add_argument('--rate', type=float, default=100.0, 
                        help='Send rate in Hz (default: 100)')
    parser.add_argument('--mode', type=str, default='rotate', 
                        choices=['rotate', 'sine', 'static', 'random'],
                        help='Simulation mode')
    parser.add_argument('--speed', type=float, default=30.0, 
                        help='Rotation speed in degrees/sec (for rotate mode)')
    args = parser.parse_args()

    # Create sockets
    data_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    discovery_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    discovery_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    
    print("=" * 60)
    print("ESP32 SIMULATOR")
    print("=" * 60)
    print(f"Target IP:       {args.target_ip}")
    print(f"Data Port:       {args.data_port}")
    print(f"Discovery Port:  {args.discovery_port}")
    print(f"Send Rate:       {args.rate:.1f} Hz")
    print(f"Mode:            {args.mode}")
    if args.mode == 'rotate':
        print(f"Rotation Speed:  {args.speed:.1f} deg/sec")
    print("=" * 60)
    print("\nPress Ctrl+C to stop\n")
    
    # Timing
    data_interval = 1.0 / args.rate
    discovery_interval = 2.0  # Send discovery every 2 seconds
    
    angle = 0.0
    last_data_time = 0.0
    last_discovery_time = 0.0
    packet_count = 0
    start_time = time.time()
    last_print_time = start_time
    
    try:
        while True:
            now = time.time()
            
            # Send discovery broadcast
            if now - last_discovery_time >= discovery_interval:
                # Use target IP for discovery (works on all platforms)
                # If target is localhost, broadcast locally; otherwise use broadcast address
                broadcast_addr = '255.255.255.255' if args.target_ip != '127.0.0.1' else '127.0.0.1'
                discovery_sock.sendto(b"DISCOVER", (broadcast_addr, args.discovery_port))
                last_discovery_time = now
            
            # Send data
            if now - last_data_time >= data_interval:
                # Update angle based on mode
                if args.mode == 'rotate':
                    # Continuous rotation
                    angle += args.speed * data_interval
                    angle = angle % 360.0
                    
                elif args.mode == 'sine':
                    # Sine wave oscillation (0-360)
                    angle = 180.0 + 180.0 * math.sin(now * 0.5)
                    
                elif args.mode == 'static':
                    # Fixed angle
                    angle = 45.0
                    
                elif args.mode == 'random':
                    # Random walk
                    import random
                    angle += random.uniform(-5.0, 5.0)
                    angle = angle % 360.0
                
                # Pack as little-endian float (same as ESP32)
                data = struct.pack('<f', angle)
                
                # Send to target
                data_sock.sendto(data, (args.target_ip, args.data_port))
                
                last_data_time = now
                packet_count += 1
                
                # Print status every second
                if now - last_print_time >= 1.0:
                    elapsed = now - start_time
                    rate = packet_count / elapsed
                    print(f"Angle: {angle:6.2f}° | Packets: {packet_count:5d} | Rate: {rate:6.1f} Hz")
                    last_print_time = now
            
            # Small sleep to avoid busy-waiting
            time.sleep(0.001)
            
    except KeyboardInterrupt:
        print("\n\nStopped by user")
        elapsed = time.time() - start_time
        avg_rate = packet_count / elapsed if elapsed > 0 else 0
        print(f"\nTotal packets sent: {packet_count}")
        print(f"Average rate: {avg_rate:.1f} Hz")
        print(f"Duration: {elapsed:.1f} seconds")
    finally:
        data_sock.close()
        discovery_sock.close()

if __name__ == "__main__":
    main()
