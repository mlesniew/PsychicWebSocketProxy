#include "server.h"

namespace PsychicWebSocketProxy {

Server::PsychicWebSocketClientProxy::PsychicWebSocketClientProxy(PsychicClient * client,
        const std::shared_ptr<Proxy> & proxy):
    PsychicWebSocketClient(client),
    proxy(proxy) {
    proxy->set_websocket_client(this);
}

Server::PsychicWebSocketClientProxy::~PsychicWebSocketClientProxy() {
    const std::shared_ptr<Proxy> ptr = proxy.lock();
    if (ptr) {
        ptr->set_websocket_client(nullptr);
    }
}

Server::Server(std::function<Proxy *()> proxy_factory) : proxy_factory(proxy_factory) {
    onFrame([this](PsychicWebSocketRequest * request, httpd_ws_frame * frame) -> esp_err_t {
        PsychicWebSocketClientProxy * client = reinterpret_cast<PsychicWebSocketClientProxy *>(PsychicWebSocketHandler::getClient(request->client()));
        const std::shared_ptr<Proxy> ptr = client->proxy.lock();
        return ptr ? ptr->recv(frame) : -1;
    });
}

Client Server::accept() {
    const std::lock_guard<std::mutex> lock(accept_mutex);
    if (!waiting_clients.empty()) {
        auto ret = waiting_clients.front();
        waiting_clients.pop_front();
        return ret;
    } else {
        return Client(nullptr);
    }
}

void Server::addClient(PsychicClient * client) {
    const std::shared_ptr<Proxy> proxy(proxy_factory());
    client->_friend = new PsychicWebSocketClientProxy(client, proxy);
    PsychicHandler::addClient(client);
    const std::lock_guard<std::mutex> lock(accept_mutex);
    waiting_clients.push_back(Client(proxy));
}

void Server::removeClient(PsychicClient * client) {
    PsychicHandler::removeClient(client);
    delete (PsychicWebSocketClientProxy *)(client->_friend);
    client->_friend = nullptr;
}

}
