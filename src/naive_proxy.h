#pragma once

#include <Arduino.h>

#include "proxy.h"

namespace PsychicWebSocketProxy {

// NOTE: This is the simplest Proxy implementation possible.  It's here for demostration purposes,
// experimenting and debugging.  Do not use it for anything serious.  It is slow, copies lots of
// buffers back and forth, causes terrible memory fragmentation and will carelessly try to allocate
// more and more memory when data is received on the websocket, but not consumed unil the heap is
// depleted and the board crashes.  But it should work OK for simple scenarios and slow connections!
class NaiveProxy: public Proxy {
    public:
        NaiveProxy(): buffer(nullptr), size(0) {}

        virtual esp_err_t recv(httpd_ws_frame * frame) {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            buffer = realloc(buffer, size + frame->len);
            memcpy(((char *) buffer) + size, frame->payload, frame->len);
            size += frame->len;
            return ESP_OK;
        }

        virtual int available() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return size;
        }

        virtual int read(uint8_t * ptr, size_t len) {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            const size_t bytes_to_read = len < size ? len : size;
            if (bytes_to_read) {
                memcpy(ptr, buffer, bytes_to_read);
                size -= bytes_to_read;
                memmove(buffer, ((char *) buffer) + bytes_to_read, size);
                buffer = realloc(buffer, size);
            }
            return bytes_to_read;
        }

        virtual int peek() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return size ? ((unsigned char *) buffer)[0] : -1;
        }

    protected:
        std::mutex recv_mutex;
        void * buffer;
        size_t size;
};

}
