#pragma once

#include <Arduino.h>

#include "proxy.h"

namespace PsychicWebSocketProxy {

// The proxy implementation pre-allocates a buffer of the given size and keeps reusing
// it.  Subsequent chunks of data received on the websocket are written to the buffer
// one after the other.  At some point, another chunk of data will not fit into the
// remaining space of the buffer.  However, at this point, some of the data which has
// been written before may already have been consumed.  If that's the case, the unread
// contents of the buffer are shifted to the beginning of the buffer making more space
// at the end.
template <size_t size, bool skip_if_no_space=false>
class ShiftingBufferProxy: public Proxy {
    public:
        ShiftingBufferProxy(): read_ptr(buffer), write_ptr(buffer) {}

        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) override {
            const size_t frame_size = frame->len;

            const std::lock_guard<std::mutex> lock(recv_mutex);

            const size_t space_tail = (buffer + size) - write_ptr;
            const size_t space_head = read_ptr - buffer;
            const size_t space_total = space_head + space_tail;

            if (space_tail < frame_size) {
                if (space_total < frame_size) {
                    return skip_if_no_space ? ESP_OK : ESP_ERR_NO_MEM;
                }

                // there's not enough memory at the end of the buffer, but
                // we can recover some memory at the beginning
                const size_t move_size = write_ptr - read_ptr;
                memmove(buffer, read_ptr, move_size);
                write_ptr -= move_size;
                read_ptr -= move_size;
            }

            frame->payload = (uint8_t *)(read_ptr);
            esp_err_t ret = httpd_ws_recv_frame(request, frame, frame_size);
            if (ret != ESP_OK) {
                ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
            } else {
                write_ptr += frame_size;
            }
            return ret;
        }

        virtual int available() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return write_ptr - read_ptr;
        }

        virtual int read(uint8_t * ptr, size_t len) {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            const size_t bytes_available = write_ptr - read_ptr;
            const size_t bytes_to_read = len < bytes_available ? len : bytes_available;
            if (bytes_to_read) {
                memcpy(ptr, read_ptr, bytes_to_read);
                read_ptr += bytes_to_read;
            }
            return bytes_to_read;
        }

        virtual int peek() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return (write_ptr > read_ptr) ? ((unsigned char *) read_ptr)[0] : -1;
        }

    protected:
        std::mutex recv_mutex;
        char buffer[size];
        char * read_ptr;
        char * write_ptr;
};

}
