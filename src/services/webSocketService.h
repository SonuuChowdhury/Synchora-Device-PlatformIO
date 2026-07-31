#pragma once

#include <WebSocketsClient.h>

enum class WebSocketEvent {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

typedef void (*WebSocketCallbackFn)(const char* message);
typedef void (*WebSocketBinaryCallbackFn)(const uint8_t* payload, size_t length);

void webSocketEventHandler(WStype_t type, uint8_t* payload, size_t length);

class WebSocketService {
public:
    void init();
    void run();
    bool isConnected();

    void sendEvent(const char* eventType);
    void sendMessage(const char* jsonMessage);
    void sendBinary(const uint8_t* data, size_t length);

    void onConnected(WebSocketCallbackFn callback);
    void onDisconnected(WebSocketCallbackFn callback);
    void onMessage(WebSocketCallbackFn callback);
    void onBinary(WebSocketBinaryCallbackFn callback);
    void onError(WebSocketCallbackFn callback);

    static WebSocketService* _instance;

private:
    friend void webSocketEventHandler(WStype_t type, uint8_t* payload, size_t length);

    WebSocketEvent _currentState = WebSocketEvent::DISCONNECTED;
    WebSocketCallbackFn _onConnectedCb = nullptr;
    WebSocketCallbackFn _onDisconnectedCb = nullptr;
    WebSocketCallbackFn _onMessageCb = nullptr;
    WebSocketBinaryCallbackFn _onBinaryCb = nullptr;
    WebSocketCallbackFn _onErrorCb = nullptr;
};