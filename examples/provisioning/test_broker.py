#!/usr/bin/env python3
"""
MQTT v5 Test Broker for EspVault Provisioning Testing

This script creates a simple MQTT v5 broker for testing the provisioning
feature with Postman or other MQTT clients.

Requirements:
    pip install paho-mqtt

Usage:
    python test_broker.py
    
Then use Postman or mosquitto_pub to send provisioning commands.
"""

import sys
import time
import json
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: paho-mqtt not installed. Run: pip install paho-mqtt")
    sys.exit(1)


class ProvisioningTestBroker:
    """Simple MQTT v5 test broker client for provisioning testing"""
    
    def __init__(self, broker_host="localhost", broker_port=1883):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = None
        self.devices = {}  # Track device responses
        
    def on_connect(self, client, userdata, flags, rc, properties=None):
        """Callback when connected to broker"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker at {self.broker_host}:{self.broker_port}")
            print(f"  Protocol: MQTT v{properties.protocol_version if properties else '5'}")
            
            # Subscribe to all device response topics
            client.subscribe("dev/res/#", qos=1)
            print("✓ Subscribed to dev/res/# (device responses)")
            
            # Subscribe to all device config topics (for monitoring)
            client.subscribe("dev/cfg/#", qos=1)
            print("✓ Subscribed to dev/cfg/# (config commands)")
            
            print("\n" + "="*70)
            print("Broker ready! Waiting for messages...")
            print("="*70)
            
        else:
            print(f"✗ Connection failed with code {rc}")
            
    def on_disconnect(self, client, userdata, rc, properties=None):
        """Callback when disconnected from broker"""
        if rc != 0:
            print(f"⚠ Unexpected disconnection (code {rc})")
            
    def on_message(self, client, userdata, msg):
        """Callback when message received"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        
        print(f"\n[{timestamp}] 📩 Message received")
        print(f"  Topic: {msg.topic}")
        print(f"  QoS: {msg.qos}")
        print(f"  Retain: {msg.retain}")
        
        # Check if this is a response from device
        if msg.topic.startswith("dev/res/"):
            mac = msg.topic.replace("dev/res/", "")
            print(f"  Device MAC: {mac}")
            
            try:
                response = json.loads(msg.payload.decode())
                print(f"\n  Response Details:")
                print(f"  {json.dumps(response, indent=4)}")
                
                status = response.get('status', 'unknown')
                if status == 'applied':
                    print(f"\n  ✓ SUCCESS: Device {mac} provisioned successfully!")
                else:
                    print(f"\n  ✗ FAILED: Device {mac} provisioning failed: {status}")
                    
                # Store response
                self.devices[mac] = {
                    'timestamp': timestamp,
                    'status': status,
                    'response': response
                }
                
            except json.JSONDecodeError:
                print(f"  Raw payload: {msg.payload}")
                
        # Check if this is a config command (just monitoring)
        elif msg.topic.startswith("dev/cfg/"):
            mac = msg.topic.replace("dev/cfg/", "")
            print(f"  Device MAC: {mac}")
            print(f"  Config command size: {len(msg.payload)} bytes")
            
            try:
                config = json.loads(msg.payload.decode())
                print(f"\n  Config Summary:")
                print(f"    WiFi SSID: {config.get('wifi', {}).get('s', 'N/A')}")
                print(f"    MQTT URI: {config.get('mqtt', {}).get('u', 'N/A')}")
                print(f"    SSL Enabled: {config.get('mqtt', {}).get('ssl', False)}")
            except:
                print(f"  Could not parse config JSON")
        
        print("-" * 70)
        
    def send_test_config(self, mac_address, config):
        """
        Send a test provisioning configuration to a device
        
        Args:
            mac_address: Device MAC address (e.g., 'aabbccddeeff')
            config: Configuration dictionary
        """
        if not self.client or not self.client.is_connected():
            print("✗ Not connected to broker")
            return False
            
        topic = f"dev/cfg/{mac_address}"
        payload = json.dumps(config)
        
        # Create MQTT v5 properties
        properties = mqtt.Properties(mqtt.PacketTypes.PUBLISH)
        properties.ResponseTopic = f"dev/res/{mac_address}"
        properties.CorrelationData = f"test-{int(time.time())}".encode()
        
        print(f"\n📤 Sending test config to {mac_address}...")
        print(f"  Topic: {topic}")
        print(f"  Size: {len(payload)} bytes")
        
        result = self.client.publish(
            topic,
            payload,
            qos=1,
            properties=properties
        )
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print("✓ Config sent successfully")
            return True
        else:
            print(f"✗ Failed to send config: {result.rc}")
            return False
            
    def show_device_status(self):
        """Show status of all devices that responded"""
        if not self.devices:
            print("\nNo device responses received yet.")
            return
            
        print("\n" + "="*70)
        print("Device Status Summary")
        print("="*70)
        
        for mac, info in self.devices.items():
            status_icon = "✓" if info['status'] == 'applied' else "✗"
            print(f"\n{status_icon} Device: {mac}")
            print(f"  Last update: {info['timestamp']}")
            print(f"  Status: {info['status']}")
            
            mem_report = info['response'].get('mem_report', {})
            if mem_report:
                print(f"  Free heap: {mem_report.get('free_heap', 'N/A')} bytes")
                print(f"  Free PSRAM: {mem_report.get('free_psram', 'N/A')} bytes")
                
        print("="*70)
        
    def run(self):
        """Start the test broker client"""
        print("\n" + "="*70)
        print("EspVault Provisioning Test Broker")
        print("="*70)
        print(f"Broker: {self.broker_host}:{self.broker_port}")
        print(f"Protocol: MQTT v5")
        print()
        
        # Create MQTT client with v5 protocol
        self.client = mqtt.Client(
            client_id="test_broker_monitor",
            protocol=mqtt.MQTTv5
        )
        
        # Set callbacks
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        
        try:
            # Connect to broker
            print(f"🔌 Connecting to {self.broker_host}:{self.broker_port}...")
            self.client.connect(self.broker_host, self.broker_port, keepalive=60)
            
            # Start loop
            print("\nPress Ctrl+C to stop\n")
            self.client.loop_forever()
            
        except KeyboardInterrupt:
            print("\n\n⏹ Stopping test broker...")
            self.show_device_status()
            
        except Exception as e:
            print(f"\n✗ Error: {e}")
            
        finally:
            if self.client:
                self.client.disconnect()
                print("✓ Disconnected from broker")


def print_usage():
    """Print usage examples"""
    print("\n" + "="*70)
    print("Usage Examples")
    print("="*70)
    print()
    print("1. Start the test broker (monitors all provisioning traffic):")
    print("   python test_broker.py")
    print()
    print("2. Send test config using mosquitto_pub:")
    print("   mosquitto_pub -h localhost -t 'dev/cfg/aabbccddeeff' \\")
    print("     -q 1 -V 5 \\")
    print("     -D PUBLISH response-topic 'dev/res/aabbccddeeff' \\")
    print("     -D PUBLISH correlation-data 'test-123' \\")
    print("     -m '{\"id\":100,\"wifi\":{\"s\":\"TestNet\",\"p\":\"Pass123\"},...}'")
    print()
    print("3. Test with Postman:")
    print("   - Create new MQTT request")
    print("   - Set protocol to MQTT 5.0")
    print("   - Topic: dev/cfg/{MAC}")
    print("   - Add Response Topic property: dev/res/{MAC}")
    print("   - Add Correlation Data property: test-session-123")
    print("   - Payload: JSON configuration")
    print()
    print("4. Example minimal config JSON:")
    print(json.dumps({
        "id": 100,
        "wifi": {"s": "TestNetwork", "p": "TestPass"},
        "ip": {"t": "d"},
        "mqtt": {
            "u": "mqtt://broker.local",
            "port": 1883,
            "ssl": False
        }
    }, indent=2))
    print()
    print("="*70)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description='MQTT v5 Test Broker for EspVault Provisioning',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('--broker', default='localhost',
                       help='MQTT broker hostname (default: localhost)')
    parser.add_argument('--port', type=int, default=1883,
                       help='MQTT broker port (default: 1883)')
    parser.add_argument('--usage', action='store_true',
                       help='Show usage examples and exit')
    
    args = parser.parse_args()
    
    if args.usage:
        print_usage()
        return
    
    broker = ProvisioningTestBroker(args.broker, args.port)
    broker.run()


if __name__ == '__main__':
    main()
