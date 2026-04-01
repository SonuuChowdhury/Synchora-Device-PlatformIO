#include <Arduino.h>
#include <WebSocketsClient.h>
#include "../config/webSocket.h"
#include "webSocketService.h"

WebSocketsClient webSocket;
WebSocketService* WebSocketService::_instance = nullptr;

// ============================================================================
// EVENT HANDLER (UNCHANGED)
void webSocketEventHandler(WStype_t type, uint8_t* payload, size_t length) {
  if (WebSocketService::_instance == nullptr) return;
  WebSocketService* svc = WebSocketService::_instance;

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WebSocket] ❌ Disconnected");
      svc->_currentState = WebSocketEvent::DISCONNECTED;
      if (svc->_onDisconnectedCb) svc->_onDisconnectedCb("Disconnected from server");
      break;
    case WStype_CONNECTED:
      Serial.println("[WebSocket] ✅ Connected");
      svc->_currentState = WebSocketEvent::CONNECTED;
      if (svc->_onConnectedCb) svc->_onConnectedCb("Connected to server");
      break;
    case WStype_TEXT: {
      Serial.printf("[WebSocket] 📨 Received: %s\n", payload);
      if (svc->_onMessageCb) {
        char* message = (char*)malloc(length + 1);
        memcpy(message, payload, length);
        message[length] = '\0';
        svc->_onMessageCb(message);
        free(message);
      }
      break;
    }
    case WStype_BIN: Serial.println("[WebSocket] 🔲 Binary data"); break;
    case WStype_ERROR:
      Serial.println("[WebSocket] ⚠️ Error");
      svc->_currentState = WebSocketEvent::ERROR;
      if (svc->_onErrorCb) svc->_onErrorCb("WebSocket error");
      break;
    default: break;
  }
}

// ============================================================================
// FIXED INIT - SUPPORT BOTH WS:// (HTTP) AND WSS:// (HTTPS)
void WebSocketService::init() {
  _instance = this;
  _currentState = WebSocketEvent::CONNECTING;

  const char* url = WEBSOCKET_URL;
  Serial.printf("[WebSocket] 🔌 Connecting to %s\n", url);

  // Parse the URL to determine scheme and host
  bool isSecure = strncmp(url, "wss://", 6) == 0;
  const char* hostStart = isSecure ? url + 6 : url + 5;  // Skip "wss://" or "ws://"
  
  // Extract host (everything before the next '/' or end of string)
  char host[256];
  int i = 0;
  while (hostStart[i] != '/' && hostStart[i] != '\0' && i < 255) {
    host[i] = hostStart[i];
    i++;
  }
  host[i] = '\0';
  
  const char* path = hostStart[i] == '/' ? &hostStart[i] : "/";
  
  // Connect based on scheme
  if (isSecure) {
    // WSS: Use SSL connection (port 443)
    webSocket.beginSSL(host, 443, path, NULL);  // NULL fingerprint skips cert check
    Serial.printf("[WebSocket] Using WSS (secure) connection\n");
  } else {
    // WS: Use plain HTTP connection (port 80)
    webSocket.begin(host, 80, path);
    Serial.printf("[WebSocket] Using WS (non-secure) connection\n");
  }
  
  // Additional ngrok headers
  webSocket.setExtraHeaders("ngrok-skip-browser-warning: true");
  webSocket.setExtraHeaders("User-Agent: ESP32/1.0");
  
  webSocket.onEvent(webSocketEventHandler);
  webSocket.setReconnectInterval(3000);
}

// ============================================================================
// RUN WITH DEBUG (UNCHANGED)
void WebSocketService::run() {
  webSocket.loop();
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    Serial.printf("[WebSocket] State: %d (%s), WiFi: %s\n", 
                  (int)_currentState,
                  _currentState == WebSocketEvent::CONNECTED ? "CONNECTED" :
                  _currentState == WebSocketEvent::CONNECTING ? "CONNECTING" : "DISCONNECTED/ERROR",
                  WiFi.status() == WL_CONNECTED ? "OK" : "DOWN");
    lastStatus = millis();
  }
}

// ============================================================================
// REST UNCHANGED
bool WebSocketService::isConnected() { return _currentState == WebSocketEvent::CONNECTED; }

void WebSocketService::sendEvent(const char* eventType) {
  if (!isConnected()) {
    Serial.printf("[WebSocket] ❌ Can't send '%s' - disconnected\n", eventType);
    return;
  }
  String json = "{\"event\":\"" + String(eventType) + "\"}";
  sendMessage(json.c_str());
}

void WebSocketService::sendMessage(const char* jsonMessage) {
  if (!isConnected()) {
    Serial.println("[WebSocket] ❌ Can't send - disconnected");
    return;
  }
  Serial.printf("[WebSocket] 📤 %s\n", jsonMessage);
  webSocket.sendTXT(jsonMessage);
}

void WebSocketService::onConnected(WebSocketCallbackFn cb) { _onConnectedCb = cb; }
void WebSocketService::onDisconnected(WebSocketCallbackFn cb) { _onDisconnectedCb = cb; }
void WebSocketService::onMessage(WebSocketCallbackFn cb) { _onMessageCb = cb; }
void WebSocketService::onError(WebSocketCallbackFn cb) { _onErrorCb = cb; }