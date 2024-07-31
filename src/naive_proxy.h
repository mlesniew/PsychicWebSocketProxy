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
        virtual ~NaiveProxy() { free(buffer); }

        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) override {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            char * new_buffer = (char *) realloc(buffer, size + frame->len);
            if (!new_buffer) {
                return ESP_ERR_NO_MEM;
            }
            buffer = new_buffer;
            frame->payload = (uint8_t *)(buffer + size);
            esp_err_t ret = httpd_ws_recv_frame(request, frame, frame->len);
            if (ret != ESP_OK) {
                ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
                // We failed to receive data, so the connection is already dying.  Don't bother to
                // increase size, free(buffer) will free the right amount of memory anyway.  Keeping
                // size unchanged will guarantee that whatever has been read so far can still be
                // retrived using the read() method, without returning uninitialized data.
            } else {
                size += frame->len;
            }
            return ret;
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
                memmove(buffer, buffer + bytes_to_read, size);
                buffer = (char *) realloc(buffer, size);
            }
            return bytes_to_read;
        }

        virtual int peek() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return size ? ((unsigned char *) buffer)[0] : -1;
        }

    protected:
        std::mutex recv_mutex;
        char * buffer;
        size_t size;
};

}
