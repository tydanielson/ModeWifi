# DynamoDB Single-Table Design

## Table Schema

### Primary Key
- **Partition Key**: `thing_name` (String) - Device identifier (e.g., "storyteller-van-01")
- **Sort Key**: `timestamp` (Number) - Unix timestamp in milliseconds

This design supports:
- ✅ Multiple devices in one table
- ✅ Efficient time-series queries per device
- ✅ Cost-effective (single table, no cross-table queries)

### Attributes

| Attribute | Type | Description | Example |
|-----------|------|-------------|---------|
| `thing_name` | String | Device identifier (PK) | "storyteller-van-01" |
| `timestamp` | Number | Unix timestamp in ms (SK) | 1733529600000 |
| `message_type` | String | Type of message | "telemetry", "status", "alert" |
| `battery_voltage` | Number | Battery voltage (V) | 13.2 |
| `glycol_temp` | Number | Glycol temperature (°F) | 72 |
| `pdm1` | Map | PDM1 channel states | {1: true, 2: false, ...} |
| `pdm2` | Map | PDM2 channel states | {1: true, 2: false, ...} |
| `hvac_mode` | String | Rixen HVAC mode | "heat", "cool", "off" |
| `hvac_temp` | Number | Rixen set temperature | 68 |
| `ttl` | Number | Expiration timestamp | 1736208000000 |

### Global Secondary Index (GSI)

**MessageTypeIndex**:
- **Partition Key**: `message_type` (String)
- **Sort Key**: `timestamp` (Number)
- **Projection**: ALL attributes

Enables queries like:
- "Show me all alerts from the last 24 hours"
- "Get all status updates since yesterday"
- Cross-device queries (if you add more vans)

## Query Patterns

### 1. Get Latest Telemetry for Device
```sql
-- Get last 10 telemetry records for van
Query:
  thing_name = "storyteller-van-01"
  timestamp > (now - 1 hour)
  ScanIndexForward = false  # Newest first
  Limit = 10
```

### 2. Get All Alerts
```sql
-- Get all alerts from last 24 hours (any device)
Query on MessageTypeIndex:
  message_type = "alert"
  timestamp > (now - 24 hours)
```

### 3. Get Device Status
```sql
-- Get most recent status message
Query:
  thing_name = "storyteller-van-01"
  message_type = "status"  # Filter expression
  Limit = 1
  ScanIndexForward = false
```

### 4. Time-Range Query
```sql
-- Get all data for device in date range
Query:
  thing_name = "storyteller-van-01"
  timestamp BETWEEN start AND end
```

## Sample Data

```json
{
  "thing_name": "storyteller-van-01",
  "timestamp": 1733529600000,
  "message_type": "telemetry",
  "battery_voltage": 13.2,
  "glycol_temp": 72,
  "pdm1": {
    "1": true,
    "2": false,
    "3": true,
    "4": false,
    "5": false,
    "6": false,
    "7": false,
    "8": false
  },
  "pdm2": {
    "1": false,
    "2": true,
    "3": false,
    "4": false,
    "5": false,
    "6": false,
    "7": false,
    "8": false
  },
  "hvac_mode": "heat",
  "hvac_temp": 68,
  "ttl": 1736208000000
}
```

## IoT Rule Integration

The IoT Rule automatically writes to this table:

```sql
SELECT 
  topic(2) as thing_name,           -- Extract from topic: van/THING_NAME/telemetry
  timestamp() as timestamp,         -- IoT Core timestamp
  'telemetry' as message_type,      -- Hardcode type
  * ,                               -- All payload fields
  timestamp() + 2592000000 as ttl  -- Expire after 30 days
FROM 'van/+/telemetry'
```

## Cost Optimization

### TTL Auto-Cleanup
- `ttl` attribute set to `timestamp + 30 days`
- DynamoDB automatically deletes expired items
- Keeps last 30 days of data
- **Free** - no charge for TTL deletions

### PAY_PER_REQUEST Billing
- No provisioned capacity
- Pay only for actual reads/writes
- $1.25 per million writes
- $0.25 per million reads

### Estimated Costs (1 device, 30s intervals)
- **Writes**: 86,400/month × $1.25/million = **$0.11/month**
- **Storage**: ~10MB × $0.25/GB = **$0.003/month**
- **Reads**: Minimal (only for dashboard) = **$0.01/month**
- **GSI**: Same as base table (included in writes)
- **TOTAL**: **~$0.12/month**

## Scaling Considerations

### Adding More Devices
Current design supports multiple devices:
```
thing_name = "storyteller-van-01"  # Your van
thing_name = "storyteller-van-02"  # Friend's van
thing_name = "overlander-01"       # Different vehicle
```

Each device gets its own partition - efficient queries!

### High-Volume Scenarios
If you exceed 1000 writes/second per partition:
- Add a shard suffix: `thing_name = "van-01#shard-1"`
- Rotate shards: `thing_name = "van-01#2025-12"`
- Not needed for van monitoring (1 write/30s)

## Best Practices

### ✅ DO
- Use millisecond timestamps (more precision)
- Set TTL on all items (cost control)
- Use GSI for cross-device queries
- Keep item size < 4KB (better performance)

### ❌ DON'T
- Don't use timestamp as partition key (hot partitions)
- Don't scan the whole table (use queries)
- Don't store large payloads (use S3 for that)
- Don't forget to set TTL (costs add up!)

## Future Enhancements

### 1. Additional GSIs
```hcl
# Alert severity index
global_secondary_index {
  name     = "AlertSeverityIndex"
  hash_key = "severity"  # "critical", "warning", "info"
  range_key = "timestamp"
}
```

### 2. Composite Sort Key
```
thing_name#message_type as PK
timestamp as SK
```
Eliminates need for filter expressions.

### 3. DynamoDB Streams
Enable streams to trigger Lambda for:
- Real-time alerts
- Data aggregation
- SNS notifications

## Questions?

This single-table design is production-ready and follows AWS best practices for time-series IoT data!
