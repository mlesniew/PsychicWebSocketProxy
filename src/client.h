#pragma once

#include <memory>
#include <Arduino.h>
#include "proxy.h"

namespace PsychicWebSocketProxy {

class Client: public ::Client {
    public:
        Client(const std::shared_ptr<Proxy> & proxy = nullptr): proxy(proxy) {}
        Client(const Client & other) = default;

        // dummy implementations -- they're not needed but need to be defined, because they're abstract in ::Client
        virtual int connect(IPAddress ip, uint16_t port) { return 0; }
        virtual int connect(const char * host, uint16_t port) { return 0; }
        virtual void flush() override { /* noop */ }

        // TODO: Move these functions to a cpp file?  But if they're defined here, they will most likely be inlined
        // TODO: Check if proxy is valid?  It shouldn't ever be invalid except when Client is constructed with a nullptr parameter...
        virtual size_t write(const uint8_t * buffer, size_t size) override { return proxy->send(buffer, size); }
        virtual int read(uint8_t * buffer, size_t size) override { return proxy->read(buffer, size); }
        virtual int available() override { return proxy->available(); }
        virtual int peek() override { return proxy->peek(); }
        virtual void stop() override { proxy->set_websocket_client(nullptr); }
        virtual uint8_t connected() override final { return proxy && proxy->connected(); }

        // TODO: Check other similar classes.  We might want to return true if we're disconnected, but there's still incoming data available.
        virtual operator bool() { return proxy && (proxy->available() || proxy->connected()); }

        virtual size_t write(uint8_t c) override final { return write(&c, 1); }
        virtual int read() override final {
            uint8_t c;
            return (read(&c, 1)) ? c : -1;
        }

    protected:
        const std::shared_ptr<Proxy> proxy;
};

}
