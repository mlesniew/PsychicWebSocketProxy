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

// Define a PsychicWebSocketProxy::Server object, which will connect
// the async PsychicHTTP server with the synchronous PicoMQTT library.
//
// The constructor accepts a function parameter which will be used as
// a proxy object factory.  Proxy class differ in how the received
// data is buffered.  See *_proxy.h headers for more details on each.
PsychicWebSocketProxy::Server websocket_handler;

// Alternative examples:
//   PsychicWebSocketProxy::Server websocket_handler([]{ return new PsychicWebSocketProxy::CircularBufferProxy(512, 10 * 1000, ESP_OK); });
//   PsychicWebSocketProxy::Server websocket_handler([]{ return new PsychicWebSocketProxy::SingleFrameProxy(10 * 1000); });
//
// Note that most proxy constructors take 3 parameters:
//  * (max) buffer size  -- specifies the size of the preallocated buffer or the maximum memory that can be used by one connection
//  * timeout_ms         -- specifies how long (in milliseconds) the async recv function should wait for memory to become available
//                          before failing.  Longer timeouts increase the tolerance for delays in the main loop() function, but reduce
//                          responsiveness (and with very long timeouts the stability) of the PsychicHTTP server
//  * error_on_no_memory -- specifies what ESP error code to return when out of memory or space in the buffer; this defualts to ESP_ERR_NO_MEM,
//                          but can be changed to ESP_OK to attempt to continue silencing the error.  PicoMQTT will not receive some data
//                          sent via the websocket.  This can cause a protocol error, but should be handled ok.  In case of protocol errors,
//                          PicoMQTT will remaining stable, but drop the connection.  Otherwise, the only consequence is potentially missing
//                          messages.

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

    // Some strict clients require that the websocket subprotocol is mqtt.
    // NOTE: The subprotocol must be set *before* attaching the handler to a
    // server path using server.on(...)
    websocket_handler.setSubprotocol("mqtt");

    // bind the PsychicWebSocketProxy::Server to an url like a websocket handler
    server.on("/mqtt", &websocket_handler);

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
