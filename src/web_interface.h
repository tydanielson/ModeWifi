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
    <h2>PDM1 - Lights & Pumps</h2>
    <div class="grid" id="pdm1"></div>
  </div>

  <div class="status">
    <h2>PDM2 - Fans & Power</h2>
    <div class="grid" id="pdm2"></div>
  </div>

  <div class="update">Auto-refresh every 2 seconds</div>

  <script>
    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          document.getElementById('voltage').textContent = data.voltage || '--';
          document.getElementById('temp').textContent = data.temp || '--';
          
          let pdm1html = '';
          data.pdm1.forEach(item => {
            const onClass = item.value > 0 ? 'on' : '';
            pdm1html += `<div class="item ${onClass}">
              <span class="name">${item.name}</span>
              <span class="value">${item.value}%</span>
            </div>`;
          });
          document.getElementById('pdm1').innerHTML = pdm1html;
          
          let pdm2html = '';
          data.pdm2.forEach(item => {
            const onClass = item.value > 0 ? 'on' : '';
            pdm2html += `<div class="item ${onClass}">
              <span class="name">${item.name}</span>
              <span class="value">${item.value}%</span>
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
