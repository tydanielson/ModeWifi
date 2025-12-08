# AWS IoT Security Features

## 🔒 Security Layers Implemented

### 1. **Certificate-Based Authentication** (Strongest)
- Only devices with valid X.509 certificates can connect
- Certificates are cryptographically signed by your AWS account
- Private keys must be kept secret on the device
- **Cannot be bypassed** - AWS IoT requires valid certs

### 2. **Strict IoT Policy** (Device Authorization)
```
✅ Device MUST connect with client ID: "storyteller-van-01"
✅ Device MUST have Thing name: "storyteller-van-01"
✅ Device can ONLY publish to: van/storyteller-van-01/*
✅ Device can ONLY subscribe to: van/storyteller-van-01/commands
❌ Device CANNOT access other topics or devices
```

### 3. **Optional IP Whitelisting** (Network Restriction)
- **Disabled by default** (Starlink IPs change frequently)
- To enable, add your IPs to `terraform.tfvars`:
  ```hcl
  allowed_ip_ranges = ["1.2.3.4/32"]  # Your Starlink IP
  ```
- Denies all connections from non-whitelisted IPs
- **Use with caution** - may block legitimate connections if IP changes

### 4. **DynamoDB Encryption** (Data at Rest)
- ✅ Server-side encryption enabled (AWS managed keys)
- ✅ Point-in-time recovery for backups
- All telemetry data encrypted in DynamoDB
- TTL enabled - auto-delete old data after 30 days

### 5. **CloudWatch Logging** (Monitoring & Auditing)
- IoT Rule errors logged to CloudWatch
- 7-day retention for troubleshooting
- Monitor for:
  - Failed message processing
  - DynamoDB write errors
  - Permission issues

### 6. **IAM Least Privilege** (Service Permissions)
```
IoT → DynamoDB role:
  ✅ Can: PutItem to van_telemetry table only
  ❌ Cannot: Read, Delete, or access other tables

IoT → CloudWatch role:
  ✅ Can: Write logs to /aws/iot/storyteller-van-01/errors only
  ❌ Cannot: Access other log groups
```

## 🚫 What Attackers CANNOT Do

Even if someone has your code:
- ❌ Connect without your certificates (crypto prevents this)
- ❌ Generate valid certificates (requires your AWS credentials)
- ❌ Access your DynamoDB data (different AWS account)
- ❌ Publish to your topics (requires your device certs)
- ❌ Subscribe to your telemetry (requires your device certs)

Even if someone steals your ESP32:
- ⚠️ They have your private key (stored in flash)
- ✅ You can **revoke the certificate** in AWS console
- ✅ Device immediately loses all access
- ✅ Generate new certificate and reflash ESP32

## ✅ Security Best Practices

### 1. Protect Your AWS Credentials
```bash
# Use MFA on your AWS account
aws iam enable-mfa-device ...

# Use SSO (you're already doing this!)
aws configure sso --profile van-profile
```

### 2. Rotate Certificates Annually
```bash
# Generate new certificate
terraform taint null_resource.generate_certificates
terraform apply

# Revoke old certificate in AWS console
# Reflash ESP32 with new certificate
```

### 3. Monitor CloudWatch Logs
```bash
# Check for errors
aws logs tail /aws/iot/storyteller-van-01/errors --follow --profile van-profile
```

### 4. Set Up Billing Alerts
```bash
# Get notified if spending exceeds $1/month
# (normal is $0.10/month, so this catches issues)
```

### 5. Never Commit Secrets
```
✅ Gitignored:
   - terraform.tfvars (your AWS profile)
   - certificates/ (private keys)
   - *.tfstate (resource IDs)

❌ Never commit:
   - Private keys (.pem, .key)
   - AWS credentials
   - IoT endpoint URLs (not secret, but not needed in repo)
```

## 📊 Security vs Usability Trade-offs

| Feature | Security Level | Usability Impact | Recommended |
|---------|---------------|------------------|-------------|
| Certificate Auth | ⭐⭐⭐⭐⭐ | Low - one-time setup | ✅ Always |
| Strict IoT Policy | ⭐⭐⭐⭐⭐ | None - transparent | ✅ Always |
| DynamoDB Encryption | ⭐⭐⭐⭐ | None - transparent | ✅ Always |
| CloudWatch Logging | ⭐⭐⭐ | None - debugging aid | ✅ Always |
| IP Whitelisting | ⭐⭐⭐ | High - breaks when IP changes | ❌ Optional |
| Flash Encryption (ESP32) | ⭐⭐⭐⭐ | Medium - harder to debug | ⚠️ Future |

## 🔥 Emergency Response

### If Certificate is Compromised:

1. **Revoke certificate immediately**
   ```bash
   aws iot update-certificate \
     --certificate-id <CERT_ID> \
     --new-status REVOKED \
     --profile default
   ```

2. **Check CloudWatch logs** for unauthorized activity

3. **Generate new certificate**
   ```bash
   cd terraform
   terraform taint null_resource.generate_certificates
   terraform apply
   ```

4. **Reflash ESP32** with new certificate

### If AWS Account is Compromised:

1. **Change AWS password immediately**
2. **Revoke all IAM access keys**
3. **Check CloudTrail** for unauthorized activity
4. **Delete/recreate IoT resources** if needed

## 📝 Compliance Checklist

Before deployment:
- [x] Certificates gitignored
- [x] terraform.tfvars gitignored
- [x] Strict IoT policy configured
- [x] DynamoDB encryption enabled
- [x] CloudWatch logging enabled
- [x] IAM roles follow least privilege
- [x] MFA enabled on AWS account (you should verify this)
- [ ] Billing alerts configured (recommended)
- [ ] CloudWatch alarms for IoT metrics (optional)

## 💰 Security Cost

All security features are **free or negligible**:
- Certificate auth: Free
- IoT Policy: Free
- DynamoDB encryption: Free (included)
- CloudWatch Logs: ~$0.01/month (7-day retention, minimal logs)
- Point-in-time recovery: ~$0.20/month per GB (minimal data)

**Total security overhead: < $0.05/month**

## 🎯 Threat Model Summary

### Threats Mitigated ✅
- Unauthorized device connections
- Cross-device data access
- Data tampering in transit (TLS)
- Data exposure at rest (encryption)
- Unauthorized AWS account access (SSO + MFA)

### Residual Risks ⚠️
- Physical device theft (mitigated by certificate revocation)
- AWS credential compromise (mitigated by MFA + monitoring)
- Starlink network compromise (mitigated by TLS + cert pinning)

### Out of Scope 🔍
- Van physical security (locks, alarms)
- Starlink account security (separate from this system)
- CAN bus security (no authentication on CAN)

## Questions?

See `SECURITY.md` for overall security architecture or ask!
