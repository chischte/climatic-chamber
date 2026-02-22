#include "web_server.h"
#include "controller.h"

// Cached JSON response - rebuilt only when data changes
static String cachedJsonResponse = "";
static unsigned long lastCacheUpdate = 0;
static const unsigned long CACHE_VALID_MS = 900; // Cache for 900ms (just under 1s sample rate)

static void sendJson(WiFiClient &client, const String &body) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(body);
}

static void sendHtml(WiFiClient &client, const String &body) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println(body);
}

static void serveIndex(WiFiClient &client, const WebServerConfig *config) {
  String html;
  html.reserve(256);
  html += "<!DOCTYPE html><html><head><meta "
          "charset=\"utf-8\"><title>Counter</title>";
  html += "<meta name=\"viewport\" "
          "content=\"width=device-width,initial-scale=1\"></head><body>";
  html += "<h1 id=\"count\">Counter: ";
  if (config != nullptr && config->values != nullptr && config->values_len > 0) {
    html += String(config->values[0]);
  } else {
    html += "0";
  }
  html += "</h1>";
  html += "<button id=\"inc\">Increment</button>";
  html += "<script>document.getElementById('inc').onclick=function(){fetch('/"
          "inc').then(r=>r.json()).then(j=>{document.getElementById('count')."
          "innerText='Counter: '+j.count});};</script>";
  html += "</body></html>";
  sendHtml(client, html);
}

static void handleIncrement(WiFiClient &client, const WebServerConfig *config) {
  if (config != nullptr && config->on_increment != nullptr) {
    config->on_increment();
  }
  uint16_t count = 0;
  if (config != nullptr && config->values != nullptr && config->values_len > 0) {
    count = config->values[0];
  }
  String json = "{\"count\":" + String(count) + "}";
  sendJson(client, json);
}

// Comprehensive Climate UI with all setpoints and current values
static void serveClimateUI(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  
  // Comprehensive HTML with all controls
  client.print(F("<html><head><title>Climate Control</title><meta charset='utf-8'>"));
  client.print(F("<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js'></script>"));
  client.print(F("<style>body{font:14px Arial;margin:15px;background:#f5f5f5}"));
  client.print(F(".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:15px;max-width:900px}"));
  client.print(F(".box{background:#fff;padding:15px;border-radius:5px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"));
  client.print(F("h1{font-size:18px;margin:0 0 10px;color:#333;border-bottom:2px solid #2196F3;padding-bottom:8px}"));
  client.print(F(".sp{font:22px monospace;font-weight:bold;padding:8px;background:#fff3e0;border-radius:4px;display:inline-block;margin:8px 0}"));
  client.print(F(".co2-sp{color:#d32f2f}.rh-sp{color:#1976d2}.temp-sp{color:#388e3c}"));
  client.print(F(".btn{padding:8px 16px;font-size:16px;margin:3px;border:none;border-radius:4px;cursor:pointer;color:white}"));
  client.print(F(".btn-co2{background:#f44336}.btn-co2:active{background:#d32f2f}"));
  client.print(F(".btn-rh{background:#2196F3}.btn-rh:active{background:#1976D2}"));
  client.print(F(".btn-temp{background:#4CAF50}.btn-temp:active{background:#388E3C}"));
  client.print(F(".chart-box{background:#fff;padding:15px;border-radius:5px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-top:15px;max-width:900px}"));
  client.print(F("canvas{height:150px!important}"));
  client.print(F(".val{font:16px monospace;padding:6px;margin:4px 0;background:#f9f9f9;border-left:3px solid #2196F3;padding-left:8px}"));
  client.print(F(".time{font-size:12px;color:#666;margin-top:10px}"));
  client.print(F("</style></head><body>"));
  
  client.print(F("<h1 style='border:none;font-size:24px;margin-bottom:15px'>Climate Chamber Control</h1>"));
  client.print(F("<div class='time' id='time'>Loading...</div>"));
  
  client.print(F("<div class='grid'>"));
  
  // CO2 Setpoint box
  client.print(F("<div class='box'><h1>CO2 Setpoint</h1>"));
  client.print(F("<div class='sp co2-sp' id='sp-co2'>...</div> <small>ppm</small><br>"));
  client.print(F("<button class='btn btn-co2' onclick='adj(\"co2\",-100)'>-100</button>"));
  client.print(F("<button class='btn btn-co2' onclick='adj(\"co2\",100)'>+100</button>"));
  client.print(F("</div>"));
  
  // RH Setpoint box
  client.print(F("<div class='box'><h1>RH Setpoint</h1>"));
  client.print(F("<div class='sp rh-sp' id='sp-rh'>...</div> <small>%</small><br>"));
  client.print(F("<button class='btn btn-rh' onclick='adj(\"rh\",-1)'>-1</button>"));
  client.print(F("<button class='btn btn-rh' onclick='adj(\"rh\",1)'>+1</button>"));
  client.print(F("</div>"));
  
  // Temp Setpoint box
  client.print(F("<div class='box'><h1>Temp Setpoint</h1>"));
  client.print(F("<div class='sp temp-sp' id='sp-temp'>...</div> <small>\u00b0C</small><br>"));
  client.print(F("<button class='btn btn-temp' onclick='adj(\"temp\",-1)'>-1</button>"));
  client.print(F("<button class='btn btn-temp' onclick='adj(\"temp\",1)'>+1</button>"));
  client.print(F("</div>"));
  
  client.print(F("</div>")); // end grid
  
  // Charts
  client.print(F("<div class='chart-box'><h1>CO2 (ppm)</h1><canvas id='co2Chart'></canvas></div>"));
  client.print(F("<div class='chart-box'><h1>Relative Humidity (%)</h1><canvas id='rhChart'></canvas></div>"));
  client.print(F("<div class='chart-box'><h1>Temperature (°C)</h1><canvas id='tempChart'></canvas></div>"));
  
  // Debug box
  client.print(F("<div class='box' style='margin-top:15px;max-width:900px'><h1>Debug Info</h1>"));
  client.print(F("<pre id='debug' style='font-size:11px;overflow:auto;max-height:200px;background:#f5f5f5;padding:10px'>Loading...</pre></div>"));
  
  // JavaScript
  client.print(F("<script>"));
  client.print(F("let co2Chart,rhChart,tempChart,timestamps=[];"));
  client.print(F("const cfg=(label,color,decimals)=>({type:'line',data:{labels:timestamps,datasets:[{label:label,data:[],borderColor:color,backgroundColor:color+'33',tension:0.3,fill:true}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{display:false},tooltip:{callbacks:{label:ctx=>label+': '+(decimals?ctx.parsed.y.toFixed(decimals):ctx.parsed.y)}}},scales:{x:{ticks:{maxRotation:45,minRotation:45}},y:{beginAtZero:false}}}});"));
  client.print(F("function initCharts(){if(typeof Chart==='undefined'){console.log('Chart.js not loaded yet, retrying...');document.getElementById('debug').innerText='Waiting for Chart.js...';setTimeout(initCharts,100);return;}console.log('Initializing charts...');try{co2Chart=new Chart(document.getElementById('co2Chart'),cfg('CO2','#f44336',0));rhChart=new Chart(document.getElementById('rhChart'),cfg('RH','#2196F3',1));tempChart=new Chart(document.getElementById('tempChart'),cfg('Temp','#4CAF50',1));console.log('Charts initialized');setInterval(u,3000);u();}catch(e){console.error('Chart init error:',e);document.getElementById('debug').innerText='Chart Error: '+e.message;}}")); 
  client.print(F("window.onload=initCharts;"));
  client.print(F("function u(){fetch('/api/last200').then(r=>r.json()).then(d=>{console.log('Data received:',d);"));
  // Update setpoints
  client.print(F("document.getElementById('sp-co2').innerHTML=d.setpoints.co2;"));
  client.print(F("document.getElementById('sp-rh').innerHTML=d.setpoints.rh.toFixed(1);"));
  client.print(F("document.getElementById('sp-temp').innerHTML=d.setpoints.temp.toFixed(1);"));
  // Update time
  client.print(F("let hrs=Math.floor(d.time/3600);let min=Math.floor((d.time%3600)/60);"));
  client.print(F("document.getElementById('time').innerHTML='Uptime: '+(hrs<10?'0':'')+hrs+':'+(min<10?'0':'')+min+' | Last update: '+new Date().toLocaleTimeString('de-DE',{hour:'2-digit',minute:'2-digit',hour12:false});"));
  // Generate timestamps and round values
  client.print(F("let now=new Date();timestamps=d.co2.map((_,i)=>{let t=new Date(now.getTime()-(d.co2.length-1-i)*3000);return t.getHours().toString().padStart(2,'0')+':'+t.getMinutes().toString().padStart(2,'0')+':'+t.getSeconds().toString().padStart(2,'0');});"));
  client.print(F("let co2Rounded=d.co2.map(v=>Math.round(v/50)*50);"));
  client.print(F("let rhRounded=d.rh.map(v=>Math.round(v*10)/10);"));
  client.print(F("let tempRounded=d.temp.map(v=>Math.round(v*10)/10);"));
  // Update charts
  client.print(F("if(co2Chart){console.log('Updating CO2 chart with',co2Rounded.length,'values');co2Chart.data.labels=timestamps;co2Chart.data.datasets[0].data=co2Rounded;co2Chart.update('none');}else{console.log('CO2 chart not ready');}"));
  client.print(F("if(rhChart){console.log('Updating RH chart');rhChart.data.labels=timestamps;rhChart.data.datasets[0].data=rhRounded;rhChart.update('none');}"));
  client.print(F("if(tempChart){console.log('Updating Temp chart');tempChart.data.labels=timestamps;tempChart.data.datasets[0].data=tempRounded;tempChart.update('none');}"));
  client.print(F("document.getElementById('debug').innerText='CO2[0-4]: '+co2Rounded.slice(0,5).join(', ')+'\\\\nRH[0-4]: '+rhRounded.slice(0,5).join(', ')+'\\\\nTemp[0-4]: '+tempRounded.slice(0,5).join(', ')+'\\\\nTotal: '+d.co2.length+' values';"));
  client.print(F("}).catch(e=>{console.error('Fetch error:',e);document.getElementById('debug').innerText='Error: '+e.message;});}"));
  
  client.print(F("function adj(type,delta){"));
  client.print(F("let sp,ep;"));
  client.print(F("if(type=='co2'){sp=parseInt(document.getElementById('sp-co2').innerText);sp+=delta;if(sp<400)sp=400;if(sp>10000)sp=10000;ep='/api/setpoint?value='+sp;}"));
  client.print(F("else if(type=='rh'){sp=parseFloat(document.getElementById('sp-rh').innerText);sp+=delta;if(sp<82)sp=82;if(sp>96)sp=96;ep='/api/setpoint_rh?value='+sp.toFixed(1);}"));
  client.print(F("else if(type=='temp'){sp=parseFloat(document.getElementById('sp-temp').innerText);sp+=delta;if(sp<18)sp=18;if(sp>32)sp=32;ep='/api/setpoint_temp?value='+sp.toFixed(1);}"));
  client.print(F("fetch(ep).then(r=>r.json()).then(d=>{u();}).catch(e=>alert('Error: '+e));}"));
  
  client.print(F("</script></body></html>"));
  
  client.flush();
}

// API endpoint: /api/last200 (cached, 5 values + all setpoints + timestamp)
static void handleLast200(WiFiClient &client) {
  unsigned long now = millis();
  Serial.println("API: /api/last200 requested");
  
  // Rebuild cache if expired or empty
  if (cachedJsonResponse.length() == 0 || (now - lastCacheUpdate) > CACHE_VALID_MS) {
    Serial.println("API: Rebuilding cache...");
    static float rhData[100];
    static float tempData[100];
    static int co2Data[100];
    
    controller_get_last200(rhData, tempData, co2Data);
    uint16_t co2_sp = controller_get_co2_setpoint();
    float rh_sp = controller_get_rh_setpoint();
    float temp_sp = controller_get_temp_setpoint();
    
    Serial.print("API: First 5 CO2 values: ");
    for (int i = 80; i < 85; i++) {
      Serial.print(co2Data[i]);
      Serial.print(" ");
    }
    Serial.println();
    
    // Build JSON once and cache it (20 values)
    cachedJsonResponse = "{\"co2\":[";
    for (int i = 80; i < 100; i++) {
      if (i > 80) cachedJsonResponse += ",";
      cachedJsonResponse += String(co2Data[i]);
    }
    cachedJsonResponse += "],\"rh\":[";
    for (int i = 80; i < 100; i++) {
      if (i > 80) cachedJsonResponse += ",";
      cachedJsonResponse += String(rhData[i], 1);
    }
    cachedJsonResponse += "],\"temp\":[";
    for (int i = 80; i < 100; i++) {
      if (i > 80) cachedJsonResponse += ",";
      cachedJsonResponse += String(tempData[i], 1);
    }
    cachedJsonResponse += "],\"setpoints\":{\"co2\":";
    cachedJsonResponse += String(co2_sp);
    cachedJsonResponse += ",\"rh\":";
    cachedJsonResponse += String(rh_sp, 1);
    cachedJsonResponse += ",\"temp\":";
    cachedJsonResponse += String(temp_sp, 1);
    cachedJsonResponse += "},\"time\":";
    cachedJsonResponse += String(millis() / 1000); // seconds since boot
    cachedJsonResponse += "}";
    
    lastCacheUpdate = now;
    Serial.print("API: Cache rebuilt, CO2=");
    Serial.print(co2Data[99]);
    Serial.print(", RH=");
    Serial.print(rhData[99], 1);
    Serial.print(", Temp=");
    Serial.print(tempData[99], 1);
    Serial.print(", JSON size: ");
    Serial.print(cachedJsonResponse.length());
    Serial.println(" bytes");
  } else {
    Serial.println("API: Using cached response");
  }
  
  // Send cached response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print(cachedJsonResponse);
  Serial.println("API: Response sent");
  client.flush();
}

// API endpoint: /api/setpoint?value=XXX (set CO2 setpoint)
static void handleSetpoint(WiFiClient &client, const String &query) {
  // Parse value from query string: value=XXX
  int valueIndex = query.indexOf("value=");
  uint16_t newSetpoint = 800; // default
  
  if (valueIndex >= 0) {
    String valueStr = query.substring(valueIndex + 6); // skip "value="
    // Remove anything after & or whitespace
    int endIndex = valueStr.indexOf('&');
    if (endIndex > 0) valueStr = valueStr.substring(0, endIndex);
    valueStr.trim();
    newSetpoint = valueStr.toInt();
  }
  
  // Set the new setpoint (will be clamped to 600-10000)
  controller_set_co2_setpoint(newSetpoint);
  uint16_t actualSetpoint = controller_get_co2_setpoint();
  
  // Invalidate cache so next request gets fresh data
  cachedJsonResponse = "";
  
  // Send response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print("{\"setpoint\":");
  client.print(actualSetpoint);
  client.println("}");
  client.flush();
  
  Serial.print("API: Setpoint set to ");
  Serial.println(actualSetpoint);
}

// API endpoint: /api/setpoint_rh?value=XX.X (set RH setpoint)
static void handleSetpointRH(WiFiClient &client, const String &query) {
  int valueIndex = query.indexOf("value=");
  float newSetpoint = 95.0f; // default
  
  if (valueIndex >= 0) {
    String valueStr = query.substring(valueIndex + 6);
    int endIndex = valueStr.indexOf('&');
    if (endIndex > 0) valueStr = valueStr.substring(0, endIndex);
    valueStr.trim();
    newSetpoint = valueStr.toFloat();
  }
  
  controller_set_rh_setpoint(newSetpoint);
  float actualSetpoint = controller_get_rh_setpoint();
  
  cachedJsonResponse = ""; // Invalidate cache
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print("{\"setpoint\":");
  client.print(actualSetpoint, 1);
  client.println("}");
  client.flush();
  
  Serial.print("API: RH setpoint set to ");
  Serial.println(actualSetpoint);
}

// API endpoint: /api/setpoint_temp?value=XX.X (set Temp setpoint)
static void handleSetpointTemp(WiFiClient &client, const String &query) {
  int valueIndex = query.indexOf("value=");
  float newSetpoint = 25.0f; // default
  
  if (valueIndex >= 0) {
    String valueStr = query.substring(valueIndex + 6);
    int endIndex = valueStr.indexOf('&');
    if (endIndex > 0) valueStr = valueStr.substring(0, endIndex);
    valueStr.trim();
    newSetpoint = valueStr.toFloat();
  }
  
  controller_set_temp_setpoint(newSetpoint);
  float actualSetpoint = controller_get_temp_setpoint();
  
  cachedJsonResponse = ""; // Invalidate cache
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print("{\"setpoint\":");
  client.print(actualSetpoint, 1);
  client.println("}");
  client.flush();
  
  Serial.print("API: Temp setpoint set to ");
  Serial.println(actualSetpoint);
}

void web_server_handle(const WebServerConfig *config) {
  if (config == nullptr || config->server == nullptr) {
    return;
  }

  WiFiClient client = config->server->accept();
  if (!client) {
    return;
  }

  Serial.println("Web: Client connected");

  // Read request line and headers until blank line
  String line = "";
  bool firstLine = true;
  String path;

  while (client.connected()) {
    if (!client.available()) {
      continue;
    }
    char c = client.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (firstLine) {
        firstLine = false;
        int sp1 = line.indexOf(' ');
        int sp2 = line.indexOf(' ', sp1 + 1);
        if (sp1 > 0 && sp2 > sp1) {
          path = line.substring(sp1 + 1, sp2);
        }
      }
      if (line.length() == 0) {
        // end of headers
        break;
      }
      line = "";
    } else {
      line += c;
    }
  }

  Serial.print("Web: Request path: ");
  Serial.println(path);

  // Parse path and query string
  String pathOnly = path;
  String query = "";
  int queryIndex = path.indexOf('?');
  if (queryIndex >= 0) {
    pathOnly = path.substring(0, queryIndex);
    query = path.substring(queryIndex + 1);
  }

  if (pathOnly == "/inc") {
    handleIncrement(client, config);
  } else if (pathOnly == "/api/last200") {
    handleLast200(client);
  } else if (pathOnly == "/api/setpoint") {
    handleSetpoint(client, query);
  } else if (pathOnly == "/api/setpoint_rh") {
    handleSetpointRH(client, query);
  } else if (pathOnly == "/api/setpoint_temp") {
    handleSetpointTemp(client, query);
  } else if (pathOnly == "/old") {
    // Old counter interface
    serveIndex(client, config);
  } else {
    // Default: Climate chamber UI
    serveClimateUI(client);
  }

  delay(1);
  client.stop();
  Serial.println("Web: Client disconnected");
}
