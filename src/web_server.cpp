#include "web_server.h"

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

void web_server_handle(const WebServerConfig *config) {
  if (config == nullptr || config->server == nullptr) {
    return;
  }

  WiFiClient client = config->server->accept();
  if (!client) {
    return;
  }

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

  if (path == "/inc") {
    handleIncrement(client, config);
  } else {
    serveIndex(client, config);
  }

  delay(1);
  client.stop();
}
