# PsychicWebSocketProxy

This ESP32 library allows connecting an asynchronous websocket created with the [PsychicHTTP](https://github.com/hoeken/PsychicHttp/) library with synchronous Arduino libraries.

It was primarily developed for use with the [PicoMQTT](https://github.com/mlesniew/PicoMQTT) MQTT client and broker library, but can be used in other situations where a synchronous server with an interface similar to `EthernetServer` (or `WiFiServer` or any of their `*Server` relatives) is needed.

## When is This Library Useful?

PsychicHTTP is asynchronous and relies on the ESP-IDF server API for websockets and HTTP requests.  Due to its asynchronous nature, it is incompatible with some Arduino libraries, which require synchronous operation.

PsychicWebSocketProxy acts as a bridge, making it possible to connect PsychicHTTP’s asynchronous websockets to libraries that need a synchronous server interface, such as `EthernetServer` or `WiFiServer`.

## Quickstart

For a quick overview, check the examples below or explore the [examples](examples/).

### Simple WebSocket Echo Server

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <PsychicHttp.h>
#include <PsychicWebSocketProxy.h>

// Define a PsychicHttpServer as usual
PsychicHttpServer server;

// Define a PsychicWebSocketProxy::Server object, which will connect
// the async PsychicHTTP server and expose a synchronous Server interface
PsychicWebSocketProxy::Server websocket_handler;

void setup() {
    // Connect to WiFi
    WiFi.begin("ssid", "secret");

    // Set up the server as usual
    server.listen(80);

    // Bind the PsychicWebSocketProxy::Server to a URL like a regular websocket handler
    server.on("/ws", &websocket_handler);
}

void loop() {
    // Use the websocket_handler like WiFiServer
    auto client = websocket_handler.accept();

    // Client can now be used like WiFiClient or other *Client classes

    while (client) {
        while (client.available()) {
            // Read a byte...
            const auto c = client.read();
            // ...and echo it back to the sender
            client.write(c);
        }
    }
}
```

Full example available [here](examples/basic/basic.ino).

### PicoMQTT integration

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <PicoMQTT.h>
#include <PsychicHttp.h>
#include <PsychicWebSocketProxy.h>

// Define a PsychicHttpServer as usual
PsychicHttpServer server;

// Define a PsychicWebSocketProxy::Server object, which will connect
// the async PsychicHTTP server and expose a synchronous Server interface
PsychicWebSocketProxy::Server websocket_handler;

// Initialize a PicoMQTT::Server specifying the PsychicWebSocketProxy::Server
// object as the server to use.
PicoMQTT::Server mqtt(websocket_handler);

void setup() {
    // Connect to WiFi
    WiFi.begin("ssid", "secret");

    // Set up the server as usual
    server.listen(80);

    // Specify the websocket's subprotocol
    websocket_handler.setSubprotocol("mqtt");

    // Bind the PsychicWebSocketProxy::Server to a URL like a websocket handler
    server.on("/mqtt", &websocket_handler);

    // Start the MQTT broker as usual
    mqtt.begin();
}

void loop() {
    mqtt.loop();
}
```

Full example available [here](examples/basic_picomqtt/basic_picomqtt.ino).

## Performance Tuning

When data arrives on the websocket, the PsychicHTTP library triggers a callback that handles the incoming data.  It is important that this callback processes the data and returns quickly.  Small delays are acceptable, but a callback that takes too long can slow down the server, cause instability, or even lead to crashes.

To mitigate this, PsychicWebSocketProxy queues the received data in RAM, allowing the callback to return quickly allowing the synchronous code process the data later.

Different use cases may require varying approaches to managing this queueing.  By default, the library uses a strategy that should perform relatively well in terms of speed and memory efficiency for most scenarios.  However, alternative buffer management strategies are available and can be used if needed.

This is achieved by providing an extra factory-function argument to the `PsychicWebSocketProxy::Server` object constructor.  For example, to use the `DynamicBufferProxy` implementation, the websocket handler should be defined as follows:

```cpp
PsychicWebSocketProxy::Server websocket_handler([] { return new PsychicWebSocketProxy::DynamicBufferProxy(); });
```

The different buffer strategies are defined and documented in the `*proxy.h` files under [src/PsychicWebSocketProxy](src/PsychicWebSocketProxy/).

## License

This library is open-source software licensed under GNU LGPLv3.
