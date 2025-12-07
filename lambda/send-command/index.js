const { IoTDataPlaneClient, PublishCommand } = require('@aws-sdk/client-iot-data-plane');

const THING_NAME = process.env.THING_NAME;
const IOT_ENDPOINT = process.env.IOT_ENDPOINT;

const iotClient = new IoTDataPlaneClient({
  endpoint: `https://${IOT_ENDPOINT}`
});

exports.handler = async (event) => {
  console.log('Event:', JSON.stringify(event, null, 2));
  
  try {
    // Parse request body
    let body;
    if (typeof event.body === 'string') {
      body = JSON.parse(event.body);
    } else {
      body = event.body || {};
    }
    
    // Validate command structure
    if (!body.command) {
      return {
        statusCode: 400,
        headers: {
          'Content-Type': 'application/json',
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Headers': '*',
          'Access-Control-Allow-Methods': 'POST, OPTIONS'
        },
        body: JSON.stringify({
          success: false,
          error: 'Missing required field: command',
          timestamp: new Date().toISOString()
        })
      };
    }
    
    // Build command payload
    const commandPayload = {
      command: body.command,
      parameters: body.parameters || {},
      timestamp: Date.now(),
      source: 'cloud-dashboard'
    };
    
    // Publish to IoT Core
    const topic = `van/${THING_NAME}/commands`;
    const publishParams = {
      topic: topic,
      payload: Buffer.from(JSON.stringify(commandPayload)),
      qos: 1
    };
    
    const command = new PublishCommand(publishParams);
    await iotClient.send(command);
    
    console.log(`Command published to ${topic}:`, commandPayload);
    
    return {
      statusCode: 200,
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Headers': '*',
        'Access-Control-Allow-Methods': 'POST, OPTIONS'
      },
      body: JSON.stringify({
        success: true,
        message: 'Command sent successfully',
        topic: topic,
        command: commandPayload,
        timestamp: new Date().toISOString()
      })
    };
    
  } catch (error) {
    console.error('Error:', error);
    
    return {
      statusCode: 500,
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Headers': '*',
        'Access-Control-Allow-Methods': 'POST, OPTIONS'
      },
      body: JSON.stringify({
        success: false,
        error: error.message,
        timestamp: new Date().toISOString()
      })
    };
  }
};
