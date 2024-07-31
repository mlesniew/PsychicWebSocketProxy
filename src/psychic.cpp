#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <PicoMQTT.h>
#include <PsychicHttp.h>

#ifdef HTTPS
#include <LittleFS.h>
#include <PsychicHttpsServer.h>
#endif


#include "server.h"
#include "naive_proxy.h"

#ifdef HTTPS
String server_cert;
String server_key;
PsychicHttpsServer server;
#else
PsychicHttpServer server;
#endif

PsychicWebSocketProxy::Server websocket_handler([] { return new PsychicWebSocketProxy::NaiveProxy(); });

::WiFiServer tcp_server(1883);
PicoMQTT::Server mqtt(tcp_server, websocket_handler);

void setup() {
    Serial.begin(115200);
    server.config.max_uri_handlers = 20;

    Serial.println("Connecting...");

    WiFi.begin();
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    MDNS.begin("picomqtt");

    Serial.println(WiFi.localIP());

#ifdef HTTPS
    {
        LittleFS.begin();
        File fp = LittleFS.open("/server.crt");
        if (fp) {
            server_cert = fp.readString();
            fp.close();
        }

        fp = LittleFS.open("/server.key");
        if (fp) {
            server_key = fp.readString();
            fp.close();
        }
    }
    server.listen(443, server_cert.c_str(), server_key.c_str());
#else
    server.listen(80);
#endif

    websocket_handler.setSubprotocol("mqtt");

    server.on("/mqtt", &websocket_handler);
    server.on("/hello", [](PsychicRequest * request) {
        return request->reply(200, "text/plain", "Hello world!");
    });

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
