#!/usr/bin/env python3
"""
Remote Provisioning Script for EspVault Universal Node

This script sends provisioning commands to ESP32 devices via MQTT v5.

Requirements:
    pip install paho-mqtt

Usage:
    python provision_device.py --mac aabbccddeeff --config config.json
"""

import argparse
import json
import sys
import uuid
import time
from typing import Dict, Any

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: paho-mqtt not installed. Run: pip install paho-mqtt")
    sys.exit(1)


class DeviceProvisioner:
    """Handles remote provisioning of EspVault devices"""
    
    def __init__(self, broker_host: str, broker_port: int = 1883, 
                 username: str = None, password: str = None):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.username = username
        self.password = password
        self.client = None
        self.response_received = False
        self.response_data = None
        
    def on_connect(self, client, userdata, flags, rc, properties=None):
        """Callback when connected to broker"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker at {self.broker_host}:{self.broker_port}")
        else:
            print(f"✗ Connection failed with code {rc}")
            
    def on_disconnect(self, client, userdata, rc, properties=None):
        """Callback when disconnected from broker"""
        if rc != 0:
            print(f"⚠ Unexpected disconnection (code {rc})")
            
    def on_message(self, client, userdata, msg):
        """Callback when response message received"""
        print(f"\n📩 Response received on {msg.topic}")
        
        try:
            self.response_data = json.loads(msg.payload.decode())
            self.response_received = True
            
            # Pretty print response
            print("\nResponse Details:")
            print(json.dumps(self.response_data, indent=2))
            
            # Interpret status
            status = self.response_data.get('status', 'unknown')
            if status == 'applied':
                print("\n✓ SUCCESS: Device provisioned successfully!")
                print("  Device will restart and connect to production network.")
            else:
                print(f"\n✗ FAILED: Status = {status}")
                print(f"  Details: {self.response_data.get('details', 'No details')}")
                
            # Show memory report if available
            mem_report = self.response_data.get('mem_report', {})
            if mem_report:
                print(f"\nMemory Report:")
                print(f"  Free Heap: {mem_report.get('free_heap', 0):,} bytes")
                print(f"  Free PSRAM: {mem_report.get('free_psram', 0):,} bytes")
                
        except json.JSONDecodeError as e:
            print(f"✗ Failed to parse response: {e}")
            print(f"Raw payload: {msg.payload}")
            
    def validate_config(self, config: Dict[str, Any]) -> bool:
        """Validate configuration before sending"""
        
        # Check required fields
        if 'wifi' not in config:
            print("✗ Missing 'wifi' section")
            return False
            
        if 's' not in config['wifi'] or 'p' not in config['wifi']:
            print("✗ Missing WiFi SSID ('s') or password ('p')")
            return False
            
        if 'mqtt' not in config:
            print("✗ Missing 'mqtt' section")
            return False
            
        mqtt_cfg = config['mqtt']
        if 'u' not in mqtt_cfg or 'port' not in mqtt_cfg or 'ssl' not in mqtt_cfg:
            print("✗ Missing MQTT required fields ('u', 'port', 'ssl')")
            return False
            
        # Check size limits
        payload_json = json.dumps(config)
        payload_size = len(payload_json)
        
        if payload_size > 8192:
            print(f"✗ Payload too large: {payload_size} bytes (max 8192)")
            return False
            
        # Check certificate size if SSL enabled
        if mqtt_cfg.get('ssl', False):
            cert = mqtt_cfg.get('cert', '')
            if len(cert) > 2048:
                print(f"✗ Certificate too large: {len(cert)} bytes (max 2048)")
                return False
                
        print(f"✓ Configuration validated ({payload_size} bytes)")
        return True
        
    def provision_device(self, mac_address: str, config: Dict[str, Any], 
                        timeout: int = 30) -> bool:
        """
        Send provisioning command to device
        
        Args:
            mac_address: Device MAC address (e.g., 'aabbccddeeff')
            config: Configuration dictionary
            timeout: Timeout in seconds to wait for response
            
        Returns:
            True if provisioning successful, False otherwise
        """
        
        # Validate configuration
        if not self.validate_config(config):
            return False
            
        # Normalize MAC address (remove colons/dashes)
        mac_clean = mac_address.replace(':', '').replace('-', '').lower()
        
        # Create MQTT client with v5 protocol
        self.client = mqtt.Client(
            client_id=f"provisioner_{uuid.uuid4().hex[:8]}",
            protocol=mqtt.MQTTv5
        )
        
        # Set credentials if provided
        if self.username and self.password:
            self.client.username_pw_set(self.username, self.password)
            
        # Set callbacks
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        
        try:
            # Connect to broker
            print(f"\n🔌 Connecting to {self.broker_host}:{self.broker_port}...")
            self.client.connect(self.broker_host, self.broker_port, keepalive=60)
            self.client.loop_start()
            
            # Wait for connection
            time.sleep(1)
            
            # Subscribe to response topic
            response_topic = f"dev/res/{mac_clean}"
            self.client.subscribe(response_topic, qos=1)
            print(f"📡 Subscribed to response: {response_topic}")
            
            # Generate correlation ID
            correlation_id = str(uuid.uuid4())
            
            # Prepare MQTT v5 properties
            properties = mqtt.Properties(mqtt.PacketTypes.PUBLISH)
            properties.ResponseTopic = response_topic
            properties.CorrelationData = correlation_id.encode()
            
            # Publish configuration
            config_topic = f"dev/cfg/{mac_clean}"
            payload = json.dumps(config)
            
            print(f"\n📤 Sending provisioning command...")
            print(f"  Topic: {config_topic}")
            print(f"  Correlation ID: {correlation_id}")
            print(f"  Payload size: {len(payload)} bytes")
            
            result = self.client.publish(
                config_topic,
                payload,
                qos=1,
                properties=properties
            )
            
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                print(f"✗ Publish failed with code {result.rc}")
                return False
                
            print("✓ Command sent, waiting for response...")
            
            # Wait for response
            start_time = time.time()
            while not self.response_received:
                if time.time() - start_time > timeout:
                    print(f"\n⏱ Timeout after {timeout} seconds")
                    return False
                time.sleep(0.1)
                
            # Check response status
            if self.response_data:
                return self.response_data.get('status') == 'applied'
            return False
            
        except Exception as e:
            print(f"\n✗ Error: {e}")
            return False
            
        finally:
            if self.client:
                self.client.loop_stop()
                self.client.disconnect()


def load_config_file(filepath: str) -> Dict[str, Any]:
    """Load configuration from JSON file"""
    try:
        with open(filepath, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"✗ Config file not found: {filepath}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"✗ Invalid JSON in config file: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Provision EspVault device via MQTT v5',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Provision device with config file
  python provision_device.py --mac aabbccddeeff --config device_config.json
  
  # Use custom broker
  python provision_device.py --mac aabbccddeeff --config config.json \\
      --broker staging.example.com --port 1883
  
  # With authentication
  python provision_device.py --mac aabbccddeeff --config config.json \\
      --broker staging.example.com --user admin --pass secret
        """
    )
    
    parser.add_argument('--mac', required=True,
                       help='Device MAC address (e.g., aabbccddeeff or aa:bb:cc:dd:ee:ff)')
    parser.add_argument('--config', required=True,
                       help='Path to JSON configuration file')
    parser.add_argument('--broker', default='localhost',
                       help='MQTT broker hostname (default: localhost)')
    parser.add_argument('--port', type=int, default=1883,
                       help='MQTT broker port (default: 1883)')
    parser.add_argument('--user', default=None,
                       help='MQTT username (optional)')
    parser.add_argument('--pass', dest='password', default=None,
                       help='MQTT password (optional)')
    parser.add_argument('--timeout', type=int, default=30,
                       help='Response timeout in seconds (default: 30)')
    
    args = parser.parse_args()
    
    # Load configuration
    print(f"📄 Loading configuration from {args.config}")
    config = load_config_file(args.config)
    
    # Create provisioner
    provisioner = DeviceProvisioner(
        args.broker,
        args.port,
        args.user,
        args.password
    )
    
    # Provision device
    print(f"\n🎯 Target device: {args.mac}")
    success = provisioner.provision_device(args.mac, config, args.timeout)
    
    # Exit with appropriate code
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
