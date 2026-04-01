#pragma once

#include <WebSocketsClient.h>

enum class WebSocketEvent {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  ERROR
};

typedef void (*WebSocketCallbackFn)(const char* message);

// Forward declaration for friend function
void webSocketEventHandler(WStype_t type, uint8_t* payload, size_t length);

class WebSocketService {
public:
  void init();
  void run();
  bool isConnected();
  
  // Send messages to server
  void sendEvent(const char* eventType);
  void sendMessage(const char* jsonMessage);
  
  // Callback registration for different events
  void onConnected(WebSocketCallbackFn callback);
  void onDisconnected(WebSocketCallbackFn callback);
  void onMessage(WebSocketCallbackFn callback);
  void onError(WebSocketCallbackFn callback);
  
  // Static instance for event handler access
  static WebSocketService* _instance;

private:
  friend void webSocketEventHandler(WStype_t type, uint8_t* payload, size_t length);
  
  WebSocketEvent _currentState = WebSocketEvent::DISCONNECTED;
  WebSocketCallbackFn _onConnectedCb = nullptr;
  WebSocketCallbackFn _onDisconnectedCb = nullptr;
  WebSocketCallbackFn _onMessageCb = nullptr;
  WebSocketCallbackFn _onErrorCb = nullptr;
};


