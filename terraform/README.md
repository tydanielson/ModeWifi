# AWS IoT Infrastructure for Van Control

This Terraform configuration sets up all AWS resources needed for remote van monitoring.

## Prerequisites

1. **AWS CLI configured**
   ```bash
   aws configure
   # Enter your AWS Access Key ID, Secret Key, and region
   # Your AWS Account ID is automatically detected - no need to specify it!
   ```

2. **Terraform installed**
   ```bash
   # macOS
   brew install terraform
   
   # Or download from https://www.terraform.io/downloads
   ```

## Configuration

### 1. Create your local variables file
```bash
cd terraform
cp terraform.tfvars.example terraform.tfvars
```

### 2. Edit `terraform.tfvars` with your settings
```hcl
aws_region = "us-east-1"        # Your preferred AWS region
thing_name = "my-van-name"      # Unique name for your van
```

⚠️ **IMPORTANT**: `terraform.tfvars` is gitignored and never committed!
This file contains your personal configuration.

## What Gets Created

- **IoT Thing** - Represents your van in AWS IoT
- **IoT Policy** - Permissions for publish/subscribe to topics
- **IoT Certificates** - Device authentication (private key, certificate, root CA)
- **DynamoDB Table** - Stores telemetry history (optional, for future dashboard)
- **IoT Rule** - Automatically saves telemetry to DynamoDB

## Deployment

### 1. Initialize Terraform
```bash
cd terraform
terraform init
```

### 2. Review the plan
```bash
terraform plan
```

### 3. Apply configuration
```bash
terraform apply
```

Type `yes` when prompted.

### 4. Attach certificate to thing
After applying, run the commands from the output:

```bash
# Get certificate ARN
CERT_ARN=$(cat ../certificates/certificate-info.json | jq -r '.certificateArn')

# Attach policy to certificate
aws iot attach-policy \
  --policy-name storyteller-van-01-policy \
  --target $CERT_ARN

# Attach certificate to thing
aws iot attach-thing-principal \
  --thing-name storyteller-van-01 \
  --principal $CERT_ARN
```

### 5. Get connection details
```bash
terraform output iot_endpoint
terraform output thing_name
```

## Customization

Edit `variables.tf` to change:
- `aws_region` - AWS region (default: us-east-1)
- `thing_name` - Your van's identifier (default: storyteller-van-01)

## Security Notes

⚠️ **IMPORTANT - Files that are NEVER committed**:

The following are automatically gitignored:
- `terraform.tfvars` - **Your personal AWS settings**
- `certificates/` - **Device certificates and private keys**
- `*.tfstate` - **Terraform state files** (contain resource IDs)
- `.terraform/` - **Terraform cache**

**What IS safe to commit**:
- `terraform.tfvars.example` - Template with no real values
- `*.tf` files - Infrastructure code (no account-specific info)
- `README.md` - Documentation

**Account ID**: Automatically detected via AWS CLI, never hardcoded!
- `private.pem.key` - **KEEP SECRET!** Never share or commit
- `certificate.pem.crt` - Device certificate
- `AmazonRootCA1.pem` - Amazon root CA (public)
- `certificate-info.json` - Certificate metadata

## Cost Estimate

With 1 device publishing every 30 seconds:
- **Messages**: ~86,400/month → $0.09
- **Connectivity**: 43,200 minutes → $0.003
- **DynamoDB**: Minimal with TTL cleanup → $0.01
- **Total**: ~$0.10/month

## Cleanup

To destroy all resources:
```bash
terraform destroy
```

Note: Certificates must be deactivated first:
```bash
CERT_ID=$(cat ../certificates/certificate-info.json | jq -r '.certificateId')
aws iot update-certificate --certificate-id $CERT_ID --new-status INACTIVE
```

## Next Steps

After infrastructure is deployed:
1. Copy certificate files to ESP32 code
2. Update `src/config.h` with IoT endpoint and thing name
3. Build and upload firmware
4. Test connection with AWS IoT Test client
