#include <mutex>

#include <Arduino.h>
#include <WiFi.h>

#include <PsychicHttp.h>
#include <PicoMQTT.h>

#include "server.h"
#include "naive_proxy.h"

PsychicHttpServer server;
PsychicWebSocketProxy::Server websocket_handler([] { return new PsychicWebSocketProxy::NaiveProxy(); });
PicoMQTT::Server mqtt(websocket_handler);

void setup() {
    Serial.begin(115200);
    server.config.max_uri_handlers = 20;

    Serial.println("Connecting...");

    WiFi.begin();
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    Serial.println(WiFi.localIP());

    //start the server listening on port 80 (standard HTTP port)
    server.listen(80);

    websocket_handler.setSubprotocol("mqtt");

    server.on("/ws", &websocket_handler);

    // Subscribe to a topic and attach a callback
    mqtt.subscribe("picomqtt/#", [](const char * topic, const char * payload) {
        // payload might be binary, but PicoMQTT guarantees that it's zero-terminated
        Serial.printf("Received message in topic '%s': %s\n", topic, payload);
    });

    mqtt.begin();
}

void loop() {
    mqtt.loop();
}
