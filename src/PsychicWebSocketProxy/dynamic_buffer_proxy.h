#pragma once

#include <condition_variable>
#include <list>

#include <Arduino.h>

#include "proxy.h"

namespace PsychicWebSocketProxy {

/* This Proxy implementation queues received data in dynamically allocated chunks of memory.  Each
 * time a new chunk is received, a new block of memory is allocated to store the received data.
 * After a chunk is consumed, memory is freed again.
 *
 * Additionally, the class limits the total size of all buffers to not exhaust all memory and lead to
 * instability.  It can also tolerate short periods when memory can't be allocated (i.e. when malloc
 * fails and returns NULL).
 *
 * This implementation is slow compared to others and can lead to significant RAM fragmentation.  It
 * can still be useful to reduce overall memory use and when connections are silent most of the time.
 */
class DynamicBufferProxy: public Proxy {
    protected:
        struct Chunk {
            Chunk(void * ptr, size_t size): size(size), buffer((char *) ptr) {}
            Chunk(size_t size): size(size), buffer((char *) malloc(size)) {}
            ~Chunk() { free(buffer); }

            Chunk(Chunk && other): size(other.size), buffer(other.buffer) {
                other.buffer = nullptr;
            }

            Chunk(const Chunk & other) = delete;
            Chunk & operator=(const Chunk & other) = delete;

            char * buffer;
            const size_t size;
        };

    public:
        DynamicBufferProxy(size_t max_size = 1024, unsigned long timeout_ms = 3000,
                           esp_err_t error_on_no_memory = ESP_ERR_NO_MEM):
            max_size(max_size), timeout(timeout_ms), error_on_no_memory(error_on_no_memory), offset(0) {}

        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) override {
            const size_t frame_size = frame->len;
            if (frame_size > max_size) {
                return error_on_no_memory;
            }

            std::unique_lock<std::mutex> lock(recv_mutex);

            // try creating a chunk of the desired size
            void * ptr = nullptr;

            if (!cond.wait_for(
                        lock,
                        timeout,
            [this, &ptr, frame_size]() -> bool {
            size_t current_size = 0;

            for (const auto & chunk : buffer) {
                    current_size += chunk.size;
                }

                if (current_size + frame_size > max_size) {
                    return false;
                }

                ptr = malloc(frame_size);
                return ptr;
            })) {
                // no space left in buffer
                return error_on_no_memory;
            }

            Chunk chunk(ptr, frame_size);

            frame->payload = (uint8_t *)(chunk.buffer);
            esp_err_t ret = httpd_ws_recv_frame(request, frame, frame->len);

            if (ret != ESP_OK) {
                ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
            } else {
                buffer.push_back(std::move(chunk));
            }
            return ret;
        }

        virtual int available() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            size_t ret = 0;
            for (const auto & chunk : buffer) {
                ret += chunk.size;
            }
            return ret - offset;
        }

        virtual int read(uint8_t * ptr, size_t len) {
            const std::lock_guard<std::mutex> lock(recv_mutex);

            uint8_t * write_ptr = ptr;

            while (len && !buffer.empty()) {
                const Chunk & chunk = buffer.front();
                const size_t bytes_available = chunk.size - offset;
                const size_t bytes_to_read = len < bytes_available ? len : bytes_available;

                memcpy(write_ptr, chunk.buffer + offset, bytes_to_read);
                offset += bytes_to_read;
                write_ptr += bytes_to_read;
                len -= bytes_to_read;

                if (offset >= chunk.size) {
                    // end of chunk reached, free it
                    buffer.pop_front();
                    offset = 0;
                    cond.notify_all();
                }
            }

            return write_ptr - ptr;
        }

        virtual int peek() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            if (buffer.empty()) {
                return -1;
            }
            const Chunk & chunk = buffer.front();
            return ((unsigned char *) chunk.buffer)[0];
        }

        const size_t max_size;
        const std::chrono::milliseconds timeout;
        const esp_err_t error_on_no_memory;

    protected:
        std::mutex recv_mutex;
        std::condition_variable cond;

        std::list<Chunk> buffer;
        size_t offset;
};

}
