#pragma once

#include <functional>
#include <list>
#include <memory>
#include <mutex>

#include <PsychicHttp.h>

#include "proxy.h"
#include "client.h"

namespace PsychicWebSocketProxy {

class Server: public PsychicWebSocketHandler {
    public:
        // TODO: This class's name is terrible
        class PsychicWebSocketClientProxy: public PsychicWebSocketClient {
            public:
                PsychicWebSocketClientProxy(PsychicClient * client, const std::shared_ptr<Proxy> & proxy);
                virtual ~PsychicWebSocketClientProxy();
                const std::weak_ptr<Proxy> proxy;
        };

        Server(std::function<Proxy *()> proxy_factory);
        Client accept();
        void begin() { /* noop */ }

        esp_err_t handleRequest(PsychicRequest * request) override;

    protected:
        virtual void addClient(PsychicClient * client) override;
        virtual void removeClient(PsychicClient * client) override;

        std::mutex accept_mutex;
        std::list<Client> waiting_clients;
        const std::function<Proxy *()> proxy_factory;
};

}
