// dependencies: mlesniew/PicoMQTT hoeken/PsychicHTTP
#include <Arduino.h>
#include <WiFi.h>

#include <PicoMQTT.h>
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
// the async PsychicHTTP server with the synchronous PicoMQTT library.
PsychicWebSocketProxy::Server websocket_handler;

// Initialize a PicoMQTT::Server using the PsychicWebSocketProxy::Server
// object as the server to use.
PicoMQTT::Server mqtt(websocket_handler);

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
    server.on("/mqtt", &websocket_handler);

    // some clients require that the websocket subprotocol is mqtt, this can be
    // omitted (or changed) for some clients
    websocket_handler.setSubprotocol("mqtt");

    // Subscribe to a topic and attach a callback
    mqtt.subscribe("#", [](const char * topic, const char * payload) {
        // payload might be binary, but PicoMQTT guarantees that it's zero-terminated
        static unsigned int num = 0;
        Serial.printf("Received message %u in topic '%s': %s\n", ++num, topic, payload);
    });

    mqtt.begin();
}

void loop() {
    mqtt.loop();
}
