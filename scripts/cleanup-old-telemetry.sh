#!/bin/bash
# Cleanup old DynamoDB telemetry records that don't have a TTL attribute.
# Run this ONCE after deploying the IoT Rule TTL fix to clean up historical data.
#
# Usage: ./scripts/cleanup-old-telemetry.sh [--dry-run]
#
# Requires: aws cli, jq, configured AWS profile "skadi"

set -euo pipefail

TABLE_NAME="storyteller-van-01-telemetry"
THING_NAME="storyteller-van-01"
AWS_PROFILE="skadi"
AWS_REGION="us-east-1"

# 14 days ago in milliseconds (for server_timestamp comparison)
CUTOFF_MS=$(( ($(date +%s) - 14 * 86400) * 1000 ))

DRY_RUN=false
if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=true
    echo "=== DRY RUN MODE - no records will be deleted ==="
fi

echo "Table: $TABLE_NAME"
echo "Cutoff: records with server_timestamp < $CUTOFF_MS (14 days ago)"
echo ""

# Get total count first
TOTAL=$(aws dynamodb scan \
    --table-name "$TABLE_NAME" \
    --select COUNT \
    --profile "$AWS_PROFILE" \
    --region "$AWS_REGION" \
    --output json | jq '.Count')

echo "Total records in table: $TOTAL"

DELETED=0
SCANNED=0
LAST_KEY=""

while true; do
    # Build scan command
    SCAN_ARGS=(
        --table-name "$TABLE_NAME"
        --filter-expression "attribute_not_exists(#ttl) OR server_timestamp < :cutoff"
        --expression-attribute-names '{"#ttl": "ttl"}'
        --expression-attribute-values "{\":cutoff\": {\"N\": \"$CUTOFF_MS\"}}"
        --projection-expression "thing_name,#ts"
        --expression-attribute-names '{"#ttl": "ttl", "#ts": "timestamp"}'
        --limit 25
        --profile "$AWS_PROFILE"
        --region "$AWS_REGION"
        --output json
    )
    
    if [[ -n "$LAST_KEY" ]]; then
        SCAN_ARGS+=(--exclusive-start-key "$LAST_KEY")
    fi
    
    RESULT=$(aws dynamodb scan "${SCAN_ARGS[@]}")
    
    ITEMS=$(echo "$RESULT" | jq -r '.Items // []')
    COUNT=$(echo "$ITEMS" | jq 'length')
    SCANNED=$((SCANNED + $(echo "$RESULT" | jq '.ScannedCount')))
    
    if [[ "$COUNT" -eq 0 ]]; then
        # Check if there are more pages
        LAST_KEY=$(echo "$RESULT" | jq -r '.LastEvaluatedKey // empty')
        if [[ -z "$LAST_KEY" ]]; then
            break
        fi
        continue
    fi
    
    echo "Found $COUNT records to delete (scanned $SCANNED so far)..."
    
    if [[ "$DRY_RUN" == "false" ]]; then
        # Build batch delete requests (max 25 per batch)
        DELETE_REQUESTS=$(echo "$ITEMS" | jq -c '[.[] | {DeleteRequest: {Key: {thing_name: .thing_name, timestamp: .timestamp}}}]')
        
        BATCH_JSON=$(echo "{\"$TABLE_NAME\": $DELETE_REQUESTS}" | jq -c '.')
        
        aws dynamodb batch-write-item \
            --request-items "$BATCH_JSON" \
            --profile "$AWS_PROFILE" \
            --region "$AWS_REGION" \
            --output json > /dev/null
        
        DELETED=$((DELETED + COUNT))
        echo "  Deleted $DELETED records so far..."
    else
        DELETED=$((DELETED + COUNT))
        echo "  Would delete $DELETED records so far..."
    fi
    
    # Check for pagination
    LAST_KEY=$(echo "$RESULT" | jq -r '.LastEvaluatedKey // empty')
    if [[ -z "$LAST_KEY" ]]; then
        break
    fi
    
    # Small delay to avoid throttling
    sleep 0.2
done

echo ""
echo "=== Complete ==="
echo "Scanned: $SCANNED records"
if [[ "$DRY_RUN" == "true" ]]; then
    echo "Would delete: $DELETED records"
    echo "Run without --dry-run to actually delete."
else
    echo "Deleted: $DELETED records"
fi
