#!/bin/bash

# Request ACM Certificate for Custom Domain
# This script must be run BEFORE terraform apply if you want a custom domain
# Certificate must be in us-east-1 region for CloudFront

set -e

DOMAIN="${1:-skadi.danielson.io}"
AWS_PROFILE="${2:-skadi}"
REGION="us-east-1"  # CloudFront requires certificates in us-east-1

echo "🔐 Requesting ACM Certificate for: $DOMAIN"
echo "   AWS Profile: $AWS_PROFILE"
echo "   Region: $REGION"
echo ""

# Request certificate
CERT_ARN=$(aws acm request-certificate \
  --domain-name "$DOMAIN" \
  --validation-method DNS \
  --region "$REGION" \
  --profile "$AWS_PROFILE" \
  --query 'CertificateArn' \
  --output text)

echo "✅ Certificate requested!"
echo "   ARN: $CERT_ARN"
echo ""

# Get validation DNS records
echo "📋 DNS Validation Records:"
aws acm describe-certificate \
  --certificate-arn "$CERT_ARN" \
  --region "$REGION" \
  --profile "$AWS_PROFILE" \
  --query 'Certificate.DomainValidationOptions[0].ResourceRecord' \
  --output table

echo ""
echo "📝 Next Steps:"
echo "1. Add the DNS validation record above to your danielson.io DNS"
echo "2. Wait for validation (usually 5-30 minutes)"
echo "3. Check status: aws acm describe-certificate --certificate-arn $CERT_ARN --region us-east-1 --profile $AWS_PROFILE --query 'Certificate.Status'"
echo "4. Add to terraform.tfvars:"
echo "   custom_domain = \"$DOMAIN\""
echo "   acm_certificate_arn = \"$CERT_ARN\""
echo "5. Run: terraform apply"
