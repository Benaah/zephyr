# AWS IoT Core Certificate Setup

This directory contains tools and placeholders for managing AWS IoT Core TLS certificates.

## Required Certificates

You need three certificate files to connect to AWS IoT Core:

1. **Root CA Certificate** (`AmazonRootCA1.pem`)
   - Download from: https://www.amazontrust.com/repository/AmazonRootCA1.pem
   - This is Amazon's root certificate authority

2. **Device Certificate** (`device-cert.pem`)
   - Obtained from AWS IoT Core when you create a Thing
   - Unique to your device

3. **Device Private Key** (`device-key.pem`)
   - Generated along with device certificate
   - Keep this secret and secure!

## Quick Start

### Step 1: Obtain Certificates from AWS IoT Core

```bash
# Using AWS CLI
aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile device-cert.pem \
    --private-key-outfile device-key.pem \
    --public-key-outfile device-public.pem
```

Or use the AWS IoT Console:
1. Navigate to AWS IoT Core → Security → Certificates
2. Click "Create certificate"
3. Download all three files

### Step 2: Download Root CA

```bash
# Download Amazon Root CA 1
curl -o AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem
```

### Step 3: Place Certificates in This Directory

```
certs/
├── AmazonRootCA1.pem       (Root CA)
├── device-cert.pem         (Your device certificate)
├── device-key.pem          (Your device private key)
└── generate_credentials.py (Conversion script)
```

### Step 4: Generate C Header File

```bash
# Run the conversion script
python3 generate_credentials.py \
    --ca-cert AmazonRootCA1.pem \
    --device-cert device-cert.pem \
    --device-key device-key.pem \
    --output ../src/aws_iot_credentials.h
```

This creates `src/aws_iot_credentials.h` with embedded certificates.

## Security Best Practices

⚠️ **IMPORTANT SECURITY NOTES:**

1. **Never commit certificates to Git**
   - The `.gitignore` is configured to exclude `.pem` files
   - Double-check before committing

2. **Protect private keys**
   ```bash
   chmod 600 device-key.pem
   ```

3. **Use different certificates per device**
   - Don't reuse the same certificate across multiple devices

4. **Rotate certificates regularly**
   - AWS IoT supports certificate rotation
   - Plan for periodic updates

5. **Production builds**
   - Consider using secure element/TPM for key storage
   - ESP32-S3 supports eFuse for secure storage

## AWS IoT Core Policy

Your certificate needs an attached policy. Example policy:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:REGION:ACCOUNT:client/kargo_sensor_*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Publish",
      "Resource": "arn:aws:iot:REGION:ACCOUNT:topic/kargo/sensors/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Subscribe",
      "Resource": "arn:aws:iot:REGION:ACCOUNT:topicfilter/kargo/sensors/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Receive",
      "Resource": "arn:aws:iot:REGION:ACCOUNT:topic/kargo/sensors/*"
    }
  ]
}
```

Apply the policy:

```bash
aws iot attach-policy \
    --policy-name KargoSensorPolicy \
    --target arn:aws:iot:REGION:ACCOUNT:cert/CERT_ID
```

## Troubleshooting

### Connection Fails with TLS Error

- Verify certificate format is PEM (base64 encoded)
- Check that certificates haven't expired
- Ensure the device certificate is attached to a policy
- Verify AWS IoT endpoint is correct

### "Certificate not found" Error

- Make sure all three certificate files are in the `certs/` directory
- Check file names match exactly
- Verify file permissions allow reading

### MQTT Connection Refused

- Check that the certificate is marked as "Active" in AWS IoT Console
- Verify the policy allows the connection
- Ensure the client ID matches your policy

## Alternative: Runtime Certificate Loading

For production, consider loading certificates from:
- External flash storage
- Secure element (ATECC608A)
- ESP32-S3 eFuse (one-time programmable)

This avoids embedding secrets in firmware binary.

## References

- [AWS IoT Core Documentation](https://docs.aws.amazon.com/iot/)
- [Zephyr TLS Credentials](https://docs.zephyrproject.org/latest/connectivity/networking/api/tls_credentials.html)
- [ESP32-S3 Security Features](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/index.html)
