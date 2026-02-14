#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Van Control</title>
  <style>
    * { 
      margin: 0; 
      padding: 0; 
      box-sizing: border-box;
      -webkit-tap-highlight-color: transparent;
    }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: #1a1a1a; 
      color: #fff; 
      padding: 15px;
      padding-bottom: 30px;
      max-width: 100%;
      overflow-x: hidden;
    }
    .header { 
      text-align: center; 
      margin-bottom: 20px;
      padding-bottom: 15px;
      border-bottom: 2px solid #333;
    }
    h1 { 
      color: #4CAF50;
      font-size: 28px;
      margin-bottom: 5px;
    }
    .status { 
      background: #2a2a2a; 
      padding: 15px; 
      border-radius: 10px; 
      margin-bottom: 15px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.3);
    }
    .status h2 { 
      color: #4CAF50; 
      margin-bottom: 12px; 
      font-size: 16px;
      font-weight: 600;
    }
    .vitals {
      display: flex;
      gap: 20px;
      justify-content: center;
      margin-top: 10px;
      flex-wrap: wrap;
    }
    .vital-item {
      text-align: center;
      min-width: 120px;
    }
    .vital-label {
      font-size: 12px;
      color: #888;
      margin-bottom: 5px;
    }
    .vital { 
      font-size: 32px; 
      font-weight: bold; 
      color: #4CAF50;
      display: block;
    }
    .grid { 
      display: grid; 
      grid-template-columns: repeat(auto-fill, minmax(140px, 1fr)); 
      gap: 8px;
    }
    .item { 
      background: #333; 
      padding: 12px; 
      border-radius: 8px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      transition: all 0.2s ease;
      min-height: 48px;
      touch-action: manipulation;
      cursor: pointer;
      user-select: none;
    }
    .item:hover {
      background: #3a3a3a;
    }
    .item:active {
      transform: scale(0.98);
    }
    .item.on { 
      background: #2d5016;
      box-shadow: 0 0 10px rgba(76, 175, 80, 0.3);
    }
    .name { 
      font-size: 11px;
      line-height: 1.3;
      flex: 1;
      padding-right: 8px;
      word-break: break-word;
    }
    .value { 
      font-weight: bold; 
      color: #4CAF50;
      font-size: 14px;
      white-space: nowrap;
    }
    .item.on .value { color: #8bc34a; }
    .update { 
      text-align: center; 
      color: #666; 
      margin-top: 20px;
      font-size: 11px;
      padding-bottom: 10px;
    }
    
    .master-switch {
      background: #2a4a2a;
      padding: 16px;
      border-radius: 10px;
      text-align: center;
      cursor: pointer;
      transition: all 0.2s ease;
      margin-bottom: 10px;
      font-size: 18px;
      font-weight: bold;
      border: 2px solid #4CAF50;
    }
    .master-switch:hover {
      background: #3a5a3a;
    }
    .master-switch:active {
      transform: scale(0.98);
    }
    .master-switch.on {
      background: #4CAF50;
      box-shadow: 0 0 15px rgba(76, 175, 80, 0.5);
    }
    
    @media (max-width: 480px) {
      body { padding: 10px; }
      h1 { font-size: 24px; }
      .status { padding: 12px; }
      .vital { font-size: 28px; }
      .grid { 
        grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
        gap: 6px;
      }
      .item { padding: 10px; }
    }
    
    @media (max-width: 360px) {
      .grid { 
        grid-template-columns: 1fr 1fr;
      }
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>🚐 Van Control</h1>
  </div>
  
  <div class="status">
    <h2>System Status</h2>
    <div class="vitals">
      <div class="vital-item">
        <div class="vital-label">Voltage</div>
        <span class="vital" id="voltage">--</span>
        <div class="vital-label">V</div>
      </div>
      <div class="vital-item">
        <div class="vital-label">Temperature</div>
        <span class="vital" id="temp">--</span>
        <div class="vital-label">°C</div>
      </div>
    </div>
  </div>

  <div class="status">
    <h2>💡 Lights</h2>
    <div class="master-switch" id="allLights" onclick="toggleAllLights()">
      ALL LIGHTS
    </div>
    <div class="grid" id="lights"></div>
  </div>

  <div class="status">
    <h2>PDM1 - Lights & Pumps</h2>
    <div class="grid" id="pdm1"></div>
  </div>

  <div class="status">
    <h2>PDM2 - Fans & Power</h2>
    <div class="grid" id="pdm2"></div>
  </div>

  <div class="update">Auto-refresh every 2 seconds</div>

  <script>
    const lightChannels = [
      {pdm: 1, channel: 2, name: 'CARGO_LIGHTS'},
      {pdm: 1, channel: 3, name: 'READING_LIGHT'},
      {pdm: 1, channel: 4, name: 'CABIN_LIGHTS'},
      {pdm: 1, channel: 5, name: 'AWNING_LIGHTS'}
    ];
    
    let lightStates = {};
    
    function toggleAllLights() {
      // Check if any light is on
      const anyOn = lightChannels.some(light => lightStates[`${light.pdm}-${light.channel}`] > 0);
      const newState = !anyOn;
      
      // Toggle all lights
      lightChannels.forEach(light => {
        fetch('/api/control', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ pdm: light.pdm, channel: light.channel, state: newState })
        });
      });
      
      // Refresh after short delay
      setTimeout(updateStatus, 300);
    }
    
    function toggleChannel(pdm, channel, currentState) {
      const newState = currentState === 0;
      console.log(`Toggling PDM${pdm} CH${channel} to ${newState ? 'ON' : 'OFF'}`);
      
      fetch('/api/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ pdm, channel, state: newState })
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          console.log('Command sent successfully');
          // Refresh status immediately
          setTimeout(updateStatus, 200);
        } else {
          console.error('Command failed:', data.message);
          alert('Failed to send command: ' + data.message);
        }
      })
      .catch(err => {
        console.error('Error:', err);
        alert('Network error: ' + err.message);
      });
    }
    
    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('voltage').textContent = data.voltage || '--';
          document.getElementById('temp').textContent = data.temp || '--';
          
          // Update lights section
          let lightsHtml = '';
          let anyLightOn = false;
          lightChannels.forEach(light => {
            const item = data.pdm1[light.channel - 1];
            const onClass = item.state > 0 ? 'on' : '';
            const stateText = item.state > 0 ? 'ON' : 'OFF';
            lightStates[`${light.pdm}-${light.channel}`] = item.state;
            if (item.state > 0) anyLightOn = true;
            
            lightsHtml += `<div class="item ${onClass}" onclick="toggleChannel(${light.pdm}, ${light.channel}, ${item.state})">
              <span class="name">${light.name}</span>
              <span class="value">${stateText}</span>
            </div>`;
          });
          document.getElementById('lights').innerHTML = lightsHtml;
          
          // Update master switch
          const masterSwitch = document.getElementById('allLights');
          if (anyLightOn) {
            masterSwitch.classList.add('on');
          } else {
            masterSwitch.classList.remove('on');
          }
          
          let pdm1html = '';
          data.pdm1.forEach((item, idx) => {
            const channel = idx + 1;
            const onClass = item.state > 0 ? 'on' : '';
            const stateText = item.state > 0 ? 'ON' : 'OFF';
            pdm1html += `<div class="item ${onClass}" onclick="toggleChannel(1, ${channel}, ${item.state})">
              <span class="name">${item.name}</span>
              <span class="value">${stateText}</span>
            </div>`;
          });
          document.getElementById('pdm1').innerHTML = pdm1html;
          
          let pdm2html = '';
          data.pdm2.forEach((item, idx) => {
            const channel = idx + 1;
            const onClass = item.state > 0 ? 'on' : '';
            const stateText = item.state > 0 ? 'ON' : 'OFF';
            pdm2html += `<div class="item ${onClass}" onclick="toggleChannel(2, ${channel}, ${item.state})">
              <span class="name">${item.name}</span>
              <span class="value">${stateText}</span>
            </div>`;
          });
          document.getElementById('pdm2').innerHTML = pdm2html;
        })
        .catch(err => console.error('Error:', err));
    }
    
    updateStatus();
    setInterval(updateStatus, 2000);
  </script>
</body>
</html>
)rawliteral";

#endif
