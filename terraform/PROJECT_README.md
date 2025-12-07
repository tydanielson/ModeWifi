# Van Control - AWS IoT Integration

Complete AWS infrastructure setup for remote van monitoring over Starlink.

## For Users

### Quick Start
1. **Prerequisites**: AWS account + AWS CLI configured
2. **Configure**: Copy `terraform.tfvars.example` to `terraform.tfvars`
3. **Customize**: Edit `terraform.tfvars` with your van name and region
4. **Deploy**: Run `terraform apply`
5. **Get certificates**: Certificates auto-generated in `../certificates/`

See [terraform/README.md](terraform/README.md) for detailed instructions.

### What You Need to Provide
- AWS credentials (via `aws configure`)
- Van name (in `terraform.tfvars`)
- Preferred AWS region (in `terraform.tfvars`)

**Your AWS Account ID is automatically detected** - you don't need to provide it!

## For Developers

### Project Structure
```
terraform/
├── main.tf                    # Main infrastructure code
├── variables.tf               # Variable definitions (no values)
├── outputs.tf                 # Output definitions
├── terraform.tfvars.example   # Example config (committed)
├── terraform.tfvars           # YOUR config (gitignored)
└── README.md                  # Deployment guide
```

### What's Safe to Share
✅ All `*.tf` files - Infrastructure as code (no account info)
✅ `terraform.tfvars.example` - Template with placeholders
✅ Documentation files

### What's Private (Gitignored)
❌ `terraform.tfvars` - Your personal AWS settings
❌ `certificates/` - Device private keys and certificates
❌ `*.tfstate` - Contains resource IDs and account info
❌ `.terraform/` - Terraform cache

### Account Detection
The Terraform code uses data sources to automatically detect:
- `data.aws_caller_identity.current.account_id` - Your AWS account
- `data.aws_region.current.name` - Current region

This means **no account-specific information is hardcoded**.

## Cost
- **~$0.10/month** per van with 30-second updates
- Free tier covers most usage for 1-2 devices

## Architecture
```
Van CAN Bus → ESP32 → Starlink WiFi → AWS IoT Core → DynamoDB
                   ↓
              Local WiFi AP (fallback)
```

## Next Steps
After deploying infrastructure:
1. Copy certificates to ESP32 code
2. Build ESP32 firmware with IoT support
3. Connect ESP32 to Starlink WiFi
4. Monitor via AWS IoT Test client
