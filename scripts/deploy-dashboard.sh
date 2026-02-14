#!/bin/bash
set -e

# Deploy dashboard with Cognito configuration injected
# Usage: ./scripts/deploy-dashboard.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TERRAFORM_DIR="$SCRIPT_DIR/../terraform"
DASHBOARD_DIR="$SCRIPT_DIR/../dashboard"
VERSION_FILE="$SCRIPT_DIR/../VERSION"

# Read version from VERSION file
VERSION=$(cat "$VERSION_FILE")
echo "📦 Version: $VERSION"
echo ""

echo "📋 Getting Cognito configuration from Terraform..."

cd "$TERRAFORM_DIR"

USER_POOL_ID=$(terraform output -raw cognito_user_pool_id)
CLIENT_ID=$(terraform output -raw cognito_client_id)
REGION=$(terraform output -raw aws_region 2>/dev/null || echo "us-east-1")

echo "✅ User Pool ID: $USER_POOL_ID"
echo "✅ Client ID: $CLIENT_ID"
echo "✅ Region: $REGION"

cd "$TERRAFORM_DIR"
API_GATEWAY_URL=$(terraform output -raw api_gateway_url)
COGNITO_DOMAIN=$(terraform output -raw cognito_hosted_ui_url | sed 's|https://||')

echo "✅ API Gateway URL: $API_GATEWAY_URL"
echo "✅ Cognito Domain: $COGNITO_DOMAIN"

cd "$DASHBOARD_DIR"

echo ""
echo "📝 Injecting configuration into dashboard files..."

# Inject config into login.html
sed -e "s/USER_POOL_ID_PLACEHOLDER/$USER_POOL_ID/g" \
    -e "s/CLIENT_ID_PLACEHOLDER/$CLIENT_ID/g" \
    -e "s/'us-east-1'/'$REGION'/g" \
    -e "s/v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*/v$VERSION/g" \
    login.html > login.html.tmp
mv login.html.tmp login.html

# Inject config into index.html
sed -e "s|\${API_GATEWAY_URL}|$API_GATEWAY_URL|g" \
    -e "s|\${COGNITO_DOMAIN}|$COGNITO_DOMAIN|g" \
    -e "s|\${COGNITO_CLIENT_ID}|$CLIENT_ID|g" \
    -e "s/v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*/v$VERSION/g" \
    index.html > index.html.tmp
mv index.html.tmp index.html

# Inject config into callback.html
sed -e "s|\${COGNITO_DOMAIN}|$COGNITO_DOMAIN|g" \
    -e "s|\${COGNITO_CLIENT_ID}|$CLIENT_ID|g" \
    callback.html > callback.html.tmp
mv callback.html.tmp callback.html

# Inject version into service worker
sed -e "s/v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*/v$VERSION/g" \
    sw.js > sw.js.tmp
mv sw.js.tmp sw.js

echo "✅ Configuration injected"
echo ""
echo "📤 Uploading to S3..."

cd "$TERRAFORM_DIR"
BUCKET=$(terraform output -raw webapp_bucket)
PROFILE=$(terraform output -raw aws_profile 2>/dev/null || echo "skadi")

cd "$DASHBOARD_DIR"

aws s3 cp index.html s3://$BUCKET/ --profile $PROFILE
aws s3 cp login.html s3://$BUCKET/ --profile $PROFILE
aws s3 cp callback.html s3://$BUCKET/ --profile $PROFILE
aws s3 cp sw.js s3://$BUCKET/ --profile $PROFILE

echo "✅ Files uploaded"
echo ""
echo "🔄 Invalidating CloudFront cache..."

cd "$TERRAFORM_DIR"
DISTRIBUTION_ID=$(terraform output -raw cloudfront_distribution_id)

aws cloudfront create-invalidation \
    --distribution-id $DISTRIBUTION_ID \
    --paths "/*" \
    --profile $PROFILE \
    --query 'Invalidation.Id' \
    --output text

echo "✅ CloudFront cache invalidated"
echo ""
echo "🎉 Deployment complete!"
echo "🌐 Your dashboard: https://skadi.danielson.io"
