#!/usr/bin/env python3
"""
AWS IoT Certificate Management Script for Kargo IoT Sensor

This script helps you provision and manage TLS certificates for AWS IoT Core connection.
It converts PEM certificates to C header format for embedding in the firmware.
"""

import argparse
import os
import sys

def pem_to_c_array(pem_file, var_name):
    """Convert PEM file to C array format"""
    with open(pem_file, 'r') as f:
        pem_content = f.read()
    
    # Create C array
    c_array = f'static const unsigned char {var_name}[] = {{\n'
    
    for line in pem_content.split('\n'):
        if line:
            for char in line:
                c_array += f'0x{ord(char):02x}, '
            c_array += '0x0a, '  # newline
            c_array += '\n'
    
    c_array += '0x00  /* null terminator */\n};\n\n'
    c_array += f'static const size_t {var_name}_len = sizeof({var_name});\n'
    
    return c_array

def generate_credentials_header(ca_cert, device_cert, device_key, output_file):
    """Generate C header file with all credentials"""
    
    header_content = """/*
 * AWS IoT Core Credentials
 * Auto-generated - DO NOT EDIT MANUALLY
 * 
 * SECURITY WARNING: This file contains sensitive cryptographic material.
 * Ensure this file is:
 * 1. Never committed to version control
 * 2. Protected with appropriate file system permissions
 * 3. Only included in secure build environments
 */

#ifndef AWS_IOT_CREDENTIALS_H
#define AWS_IOT_CREDENTIALS_H

#include <stddef.h>

/* AWS IoT Root CA Certificate */
"""
    
    # Add CA certificate
    if ca_cert and os.path.exists(ca_cert):
        header_content += pem_to_c_array(ca_cert, 'aws_iot_root_ca')
    else:
        print(f"Warning: CA certificate not found: {ca_cert}")
    
    header_content += "\n/* Device Certificate */\n"
    
    # Add device certificate
    if device_cert and os.path.exists(device_cert):
        header_content += pem_to_c_array(device_cert, 'aws_iot_device_cert')
    else:
        print(f"Warning: Device certificate not found: {device_cert}")
    
    header_content += "\n/* Device Private Key */\n"
    
    # Add device private key
    if device_key and os.path.exists(device_key):
        header_content += pem_to_c_array(device_key, 'aws_iot_device_key')
    else:
        print(f"Warning: Device private key not found: {device_key}")
    
    header_content += """
/* Helper function to load credentials into TLS */
static inline int load_aws_iot_credentials(int sec_tag)
{
    int err;
    
    /* Load CA certificate */
    err = tls_credential_add(sec_tag, TLS_CREDENTIAL_CA_CERTIFICATE,
                             aws_iot_root_ca, aws_iot_root_ca_len);
    if (err < 0) {
        return err;
    }
    
    /* Load device certificate */
    err = tls_credential_add(sec_tag, TLS_CREDENTIAL_SERVER_CERTIFICATE,
                             aws_iot_device_cert, aws_iot_device_cert_len);
    if (err < 0) {
        return err;
    }
    
    /* Load device private key */
    err = tls_credential_add(sec_tag, TLS_CREDENTIAL_PRIVATE_KEY,
                             aws_iot_device_key, aws_iot_device_key_len);
    if (err < 0) {
        return err;
    }
    
    return 0;
}

#endif /* AWS_IOT_CREDENTIALS_H */
"""
    
    # Write to output file
    with open(output_file, 'w') as f:
        f.write(header_content)
    
    print(f"Credentials header generated: {output_file}")
    print("\nNext steps:")
    print("1. Include this header in your main.c: #include \"aws_iot_credentials.h\"")
    print("2. Call load_aws_iot_credentials(CONFIG_AWS_IOT_SEC_TAG) before MQTT connect")
    print("3. Update prj.conf with your AWS IoT endpoint and client ID")

def main():
    parser = argparse.ArgumentParser(
        description='Convert AWS IoT certificates to C header format'
    )
    parser.add_argument('--ca-cert', required=True,
                       help='Path to AWS IoT Root CA certificate (AmazonRootCA1.pem)')
    parser.add_argument('--device-cert', required=True,
                       help='Path to device certificate')
    parser.add_argument('--device-key', required=True,
                       help='Path to device private key')
    parser.add_argument('--output', default='src/aws_iot_credentials.h',
                       help='Output C header file path')
    
    args = parser.parse_args()
    
    generate_credentials_header(
        args.ca_cert,
        args.device_cert,
        args.device_key,
        args.output
    )

if __name__ == '__main__':
    main()
