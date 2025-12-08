// Lambda function to get telemetry data from DynamoDB (with GSI support)
const { DynamoDBClient } = require('@aws-sdk/client-dynamodb');
const { DynamoDBDocumentClient, ScanCommand, QueryCommand } = require('@aws-sdk/lib-dynamodb');

const client = new DynamoDBClient({});
const docClient = DynamoDBDocumentClient.from(client);

const TABLE_NAME = process.env.TABLE_NAME;
const THING_NAME = process.env.THING_NAME;

exports.handler = async (event) => {
  console.log('Event:', JSON.stringify(event, null, 2));
  
  try {
    const path = event.rawPath || event.path || '';
    const queryParams = event.queryStringParameters || {};
    
    // Determine if this is a "latest" request
    const isLatest = path.includes('/latest');
    
    if (isLatest) {
      // Get the most recent telemetry record
      const params = {
        TableName: TABLE_NAME,
        Limit: 1,
        ScanIndexForward: false, // descending order
      };
      
      // Query using GSI on server_timestamp for efficient latest record lookup
      const queryParams = {
        TableName: TABLE_NAME,
        IndexName: 'ThingServerTimestampIndex',
        KeyConditionExpression: 'thing_name = :thingName',
        FilterExpression: 'message_type = :msgType',
        ExpressionAttributeValues: {
          ':thingName': THING_NAME,
          ':msgType': 'telemetry'
        },
        ScanIndexForward: false, // Descending order (newest first)
        Limit: 1 // Only need the latest record
      };
      
      const command = new QueryCommand(queryParams);
      const response = await docClient.send(command);
      
      const latest = response.Items && response.Items.length > 0 ? response.Items[0] : null;
      
      // Convert server_timestamp from milliseconds to ISO string if present
      let serverTimestamp = new Date().toISOString();
      if (latest?.server_timestamp) {
        serverTimestamp = new Date(parseInt(latest.server_timestamp)).toISOString();
      }
      
      return {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json',
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Headers': '*',
          'Access-Control-Allow-Methods': 'GET, OPTIONS'
        },
        body: JSON.stringify({
          success: true,
          data: latest,
          server_timestamp: serverTimestamp,
          api_timestamp: new Date().toISOString()
        })
      };
      
    } else {
      // Get historical telemetry data
      const limit = parseInt(queryParams.limit) || 100;
      const startTime = queryParams.startTime;
      const endTime = queryParams.endTime;
      
      const scanParams = {
        TableName: TABLE_NAME,
        FilterExpression: 'thing_name = :thingName AND message_type = :msgType',
        ExpressionAttributeValues: {
          ':thingName': THING_NAME,
          ':msgType': 'telemetry'
        },
        Limit: Math.min(limit, 1000) // Cap at 1000 records
      };
      
      // Add timestamp filtering if provided
      if (startTime || endTime) {
        if (startTime && endTime) {
          scanParams.FilterExpression += ' AND #ts BETWEEN :start AND :end';
          scanParams.ExpressionAttributeValues[':start'] = startTime;
          scanParams.ExpressionAttributeValues[':end'] = endTime;
          scanParams.ExpressionAttributeNames = { '#ts': 'timestamp' };
        } else if (startTime) {
          scanParams.FilterExpression += ' AND #ts >= :start';
          scanParams.ExpressionAttributeValues[':start'] = startTime;
          scanParams.ExpressionAttributeNames = { '#ts': 'timestamp' };
        } else if (endTime) {
          scanParams.FilterExpression += ' AND #ts <= :end';
          scanParams.ExpressionAttributeValues[':end'] = endTime;
          scanParams.ExpressionAttributeNames = { '#ts': 'timestamp' };
        }
      }
      
      const command = new ScanCommand(scanParams);
      const response = await docClient.send(command);
      
      // Sort by timestamp (descending)
      const items = response.Items || [];
      items.sort((a, b) => {
        const aTime = parseInt(a.timestamp) || 0;
        const bTime = parseInt(b.timestamp) || 0;
        return bTime - aTime;
      });
      
      return {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json',
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Headers': '*',
          'Access-Control-Allow-Methods': 'GET, OPTIONS'
        },
        body: JSON.stringify({
          success: true,
          count: items.length,
          data: items,
          api_timestamp: new Date().toISOString()
        })
      };
    }
    
  } catch (error) {
    console.error('Error:', error);
    
    return {
      statusCode: 500,
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Headers': '*',
        'Access-Control-Allow-Methods': 'GET, OPTIONS'
      },
      body: JSON.stringify({
        success: false,
        error: error.message,
        timestamp: new Date().toISOString()
      })
    };
  }
};
