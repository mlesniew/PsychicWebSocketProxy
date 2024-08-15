#include <Arduino.h>
#include <WiFi.h>

#include <PsychicHttp.h>
#include <PsychicWebSocketProxy.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "WiFi SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "password"
#endif

// define a PsychicWebSocketProxy::Server object, which will connect
// the async PsychicHTTP server and expose a synchronous Server interface
PsychicWebSocketProxy::Server websocket_handler([] { return new PsychicWebSocketProxy::DynamicBufferProxy(); });

// Setup a PsychicHttpServer as usual
PsychicHttpServer server;

void setup() {
    // Setup serial
    Serial.begin(115200);

    // Connect to WiFi
    Serial.printf("Connecting to WiFi %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { delay(1000); }
    Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

    // Setup server
    server.config.max_uri_handlers = 20;
    server.listen(80);

    // bind the PsychicWebSocketProxy::Server to an url like a websocket handler
    server.on("/ws", &websocket_handler);
}

void loop() {
    // use the websocket_handler like WiFiServer
    auto client = websocket_handler.accept();
    while (client) {
        const size_t bytes_available = client.available();
        if (!bytes_available) {
            continue;
        }
        const size_t size = bytes_available <= 1024 ? bytes_available : 1024;
        char buffer[size + 1];
        memset(buffer, 0, size + 1);

        client.read((uint8_t *) buffer, size);
        client.write((uint8_t *) buffer, size);
        Serial.print(buffer);
    }
}
