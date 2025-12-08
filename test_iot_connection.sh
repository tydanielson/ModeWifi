#!/bin/bash
# Test AWS IoT Core connectivity using mosquitto_pub
# This simulates what the ESP32 will do

set -e

THING_NAME="storyteller-van-01"
IOT_ENDPOINT="aapx64lmescn4-ats.iot.us-east-1.amazonaws.com"
CERT_DIR="./certificates"

# Check if mosquitto_pub is installed
if ! command -v mosquitto_pub &> /dev/null; then
    echo "❌ mosquitto_pub not found. Install with:"
    echo "   brew install mosquitto"
    exit 1
fi

# Check if certificates exist
if [ ! -f "$CERT_DIR/certificate.pem.crt" ]; then
    echo "❌ Certificates not found in $CERT_DIR"
    echo "   Run terraform apply first to generate certificates"
    exit 1
fi

echo "🧪 Testing AWS IoT Core connection..."
echo "📡 Endpoint: $IOT_ENDPOINT"
echo "🔐 Thing: $THING_NAME"
echo ""

# Test payload - simulating ESP32 telemetry
TIMESTAMP=$(python3 -c "import time; print(int(time.time() * 1000))")
TEST_PAYLOAD=$(cat <<EOF
{
  "thing_name": "$THING_NAME",
  "timestamp": $TIMESTAMP,
  "message_type": "telemetry",
  "battery_voltage": 13.2,
  "glycol_temp": 72,
  "pdm1": [true, false, true, false, false, false, false, false],
  "pdm2": [false, true, false, false, false, false, false, false],
  "hvac_mode": "heat",
  "hvac_temp": 68
}
EOF
)

echo "📤 Publishing test message to: van/$THING_NAME/telemetry"
echo "Payload:"
echo "$TEST_PAYLOAD" | jq .
echo ""

# Publish to AWS IoT
mosquitto_pub \
  --cafile "$CERT_DIR/AmazonRootCA1.pem" \
  --cert "$CERT_DIR/certificate.pem.crt" \
  --key "$CERT_DIR/private.pem.key" \
  -h "$IOT_ENDPOINT" \
  -p 8883 \
  -q 1 \
  -t "van/$THING_NAME/telemetry" \
  -i "$THING_NAME" \
  -m "$TEST_PAYLOAD" \
  -d

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Message published successfully!"
    echo ""
    echo "🔍 Next steps:"
    echo "1. Go to AWS Console → IoT Core → Test → MQTT test client"
    echo "2. Subscribe to: van/$THING_NAME/#"
    echo "3. Run this script again to see messages appear"
    echo ""
    echo "4. Check DynamoDB:"
    echo "   aws dynamodb scan --table-name $THING_NAME-telemetry --profile default | jq '.Items[0]'"
else
    echo ""
    echo "❌ Failed to publish message"
    echo "Check certificates and IoT endpoint"
fi
