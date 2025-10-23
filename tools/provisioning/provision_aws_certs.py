#!/usr/bin/env python3
"""
AWS IoT Certificate Provisioning Tool for KargoPod
SPDX-License-Identifier: Apache-2.0
Copyright (c) 2025 Kargo Chain
"""

import argparse
import os
import sys
import subprocess
from pathlib import Path

def provision_certificates(device_id, cert_dir, sec_tag=1):
    """
    Provision AWS IoT certificates to device
    
    Args:
        device_id: Device identifier
        cert_dir: Directory containing certificates
        sec_tag: Security tag number (default: 1)
    """
    print(f"Provisioning certificates for device: {device_id}")
    print(f"Certificate directory: {cert_dir}")
    print(f"Security tag: {sec_tag}")
    
    cert_path = Path(cert_dir)
    
    # Expected certificate files
    ca_cert = cert_path / "AmazonRootCA1.pem"
    device_cert = cert_path / f"{device_id}-certificate.pem.crt"
    private_key = cert_path / f"{device_id}-private.pem.key"
    
    # Verify files exist
    for cert_file in [ca_cert, device_cert, private_key]:
        if not cert_file.exists():
            print(f"Error: Certificate file not found: {cert_file}")
            return False
    
    print("\nCertificate files found:")
    print(f"  CA: {ca_cert}")
    print(f"  Device cert: {device_cert}")
    print(f"  Private key: {private_key}")
    
    # For nRF9160: Use nRF Connect SDK's modem key management
    if sys.platform == "linux" or sys.platform == "darwin":
        print("\nProvisioning to nRF9160 modem...")
        try:
            # Write CA certificate
            subprocess.run([
                "nrfjprog", "--program", str(ca_cert),
                "--sectag", str(sec_tag), "--credtype", "0"
            ], check=True)
            
            # Write device certificate
            subprocess.run([
                "nrfjprog", "--program", str(device_cert),
                "--sectag", str(sec_tag), "--credtype", "1"
            ], check=True)
            
            # Write private key
            subprocess.run([
                "nrfjprog", "--program", str(private_key),
                "--sectag", str(sec_tag), "--credtype", "2"
            ], check=True)
            
            print("Certificates provisioned successfully!")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Error provisioning certificates: {e}")
            return False
    
    # For ESP32-S3: Generate C header file with certificates
    print("\nGenerating certificate header for ESP32-S3...")
    header_file = cert_path / "aws_iot_certs.h"
    
    try:
        with open(header_file, 'w') as hf:
            hf.write("/* Auto-generated AWS IoT certificates */\n")
            hf.write("/* SPDX-License-Identifier: Apache-2.0 */\n\n")
            hf.write("#ifndef AWS_IOT_CERTS_H_\n")
            hf.write("#define AWS_IOT_CERTS_H_\n\n")
            
            # CA certificate
            with open(ca_cert, 'r') as cf:
                ca_content = cf.read()
                hf.write('static const char aws_iot_ca_cert[] = \n')
                for line in ca_content.split('\n'):
                    hf.write(f'    "{line}\\n"\n')
                hf.write('    ;\n\n')
            
            # Device certificate
            with open(device_cert, 'r') as cf:
                cert_content = cf.read()
                hf.write('static const char aws_iot_device_cert[] = \n')
                for line in cert_content.split('\n'):
                    hf.write(f'    "{line}\\n"\n')
                hf.write('    ;\n\n')
            
            # Private key
            with open(private_key, 'r') as cf:
                key_content = cf.read()
                hf.write('static const char aws_iot_private_key[] = \n')
                for line in key_content.split('\n'):
                    hf.write(f'    "{line}\\n"\n')
                hf.write('    ;\n\n')
            
            hf.write("#endif /* AWS_IOT_CERTS_H_ */\n")
        
        print(f"Certificate header generated: {header_file}")
        print("\nNext steps:")
        print("1. Copy aws_iot_certs.h to your project's src/ directory")
        print("2. Include it in your MQTT manager code")
        print("3. Register certificates with tls_credential_add()")
        
        return True
        
    except Exception as e:
        print(f"Error generating header file: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="Provision AWS IoT certificates to KargoPod device"
    )
    parser.add_argument(
        "device_id",
        help="Device identifier (e.g., kargopod-001)"
    )
    parser.add_argument(
        "cert_dir",
        help="Directory containing AWS IoT certificates"
    )
    parser.add_argument(
        "--sec-tag",
        type=int,
        default=1,
        help="Security tag number (default: 1)"
    )
    
    args = parser.parse_args()
    
    if provision_certificates(args.device_id, args.cert_dir, args.sec_tag):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
