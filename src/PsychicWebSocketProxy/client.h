#pragma once

#include <memory>
#include <Arduino.h>
#include "proxy.h"

namespace PsychicWebSocketProxy {

class Client: public ::Client {
    public:
        Client(const std::shared_ptr<Proxy> & proxy = nullptr): proxy(proxy) {}
        Client(const Client & other) = default;

        // dummy implementations -- they're not needed but have to be defined, because they're abstract in ::Client
        virtual int connect(IPAddress ip, uint16_t port) { return 0; }
        virtual int connect(const char * host, uint16_t port) { return 0; }
        virtual void flush() override { /* noop */ }

        // NOTE: The methods below access proxy without checking for NULL.  The check is skipped for speed.
        // At the same time, it's safe to not check it, because proxy can only be NULL if the object is initialized
        // with null.  An object like that can only be returned by Server::accept() when no new client has connected.
        // However, in that case the natural first thing the caller of Server::accept() would do is checking if the
        // client is valid using the connected() method or by convertingt to bool.  These two methods have the NULL
        // check in place.
        virtual size_t write(const uint8_t * buffer, size_t size) override { return proxy->send(buffer, size); }
        virtual int read(uint8_t * buffer, size_t size) override { return proxy->read(buffer, size); }
        virtual int available() override { return proxy->available(); }
        virtual int peek() override { return proxy->peek(); }
        virtual void stop() override { proxy->set_websocket_client(nullptr); }
        virtual uint8_t connected() override final { return proxy && proxy->connected(); }

        // NOTE: This is implemented in the same way as in WiFiClient -- returns true if we're connected or if there's still some unread data remaining
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
