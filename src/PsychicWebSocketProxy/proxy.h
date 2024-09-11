#pragma once
#include <mutex>

#include <PsychicHttp.h>

namespace PsychicWebSocketProxy {

class Proxy {
    public:
        Proxy(): psychic_client(nullptr) {}

        Proxy(const Proxy & other) = delete;
        const Proxy & operator=(const Proxy & other) = delete;

        virtual ~Proxy() {}

        void set_websocket_client(PsychicWebSocketClient * psychic_client) {
            const std::lock_guard<std::mutex> lock(send_mutex);
            this->psychic_client = psychic_client;
        }

        size_t send(const void * buf, const size_t len) {
            const std::lock_guard<std::mutex> lock(send_mutex);
            if (psychic_client && psychic_client->sendMessage(HTTPD_WS_TYPE_BINARY, buf, len) == ESP_OK) {
                return len;
            } else {
                return 0;
            }
        }

        virtual uint8_t connected() {
            const std::lock_guard<std::mutex> lock(send_mutex);
            // The psychic_client is set to NULL when the connection managed by PsychicHttp dies.
            // As long as it's not NULL, we're connected.
            return bool(psychic_client);
        }

        // this iss called from the event loop running the server
        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) = 0;

        // these are called from the main loop
        virtual int available() = 0;
        virtual int read(uint8_t * buffer, size_t size) = 0;
        virtual int peek() = 0;

    protected:
        std::mutex send_mutex;
        PsychicWebSocketClient * psychic_client;
};

}
