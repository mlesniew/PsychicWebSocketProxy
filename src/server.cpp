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

Server::Server(std::function<Proxy *()> proxy_factory) : proxy_factory(proxy_factory) {}

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

esp_err_t Server::handleRequest(PsychicRequest * request) {
    // lookup our client
    PsychicClient * client = checkForNewClient(request->client());

    // beginning of the ws URI handler and our onConnect hook
    if (request->method() == HTTP_GET) {
        if (client->isNew) {
            openCallback(client);
        }
        return ESP_OK;
    }

    // prep our request
    PsychicWebSocketRequest wsRequest(request);

    // init our memory for storing the packet
    httpd_ws_frame_t ws_pkt = { .len = 0 };

    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(wsRequest.request(), &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed to get frame len with %s", esp_err_to_name(ret));
        return ret;
    }

    // okay, now try to load the packet
    ESP_LOGI(PH_TAG, "frame len is %d", ws_pkt.len);
    if (!ws_pkt.len) {
        return ESP_OK;
    }

    ws_pkt.payload = (uint8_t *) malloc(ws_pkt.len);
    if (!ws_pkt.payload) {
        ESP_LOGE(PH_TAG, "Failed to calloc memory for buf");
        return ESP_ERR_NO_MEM;
    }

    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(wsRequest.request(), &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
        free(ws_pkt.payload);
        return ret;
    }

    // the code below replaces
    //     ret = this->_onFrame(&wsRequest, &ws_pkt);
    {
        PsychicWebSocketClientProxy * client = reinterpret_cast<PsychicWebSocketClientProxy *>
                                               (PsychicWebSocketHandler::getClient(wsRequest.client()));
        const std::shared_ptr<Proxy> ptr = client->proxy.lock();
        ret = ptr ? ptr->recv(&ws_pkt) : -1;
    }

    // logging housekeeping
    if (ret != ESP_OK) {
        ESP_LOGE(PH_TAG, "httpd_ws_send_frame failed with %s", esp_err_to_name(ret));
    }

    ESP_LOGI(PH_TAG, "ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", request->server(),
             httpd_req_to_sockfd(request->request()), httpd_ws_get_fd_info(request->server(),
                     httpd_req_to_sockfd(request->request())));

    // dont forget to release our buffer memory
    free(ws_pkt.payload);

    return ret;
}

}
