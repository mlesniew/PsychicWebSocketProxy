#pragma once

#include <Arduino.h>

#include <condition_variable>

#include "proxy.h"

namespace PsychicWebSocketProxy {

/* This is a very simple proxy, which preallocates a buffer of fixed size for recevied
 * data.  It maintains two pointers: write_ptr to track where to write received data
 * and read_ptr to track where to read buffered data from.
 *
 * At the beginning write_ptr and read_ptr both point at the beginning of the buffer.
 *
 * When new data is received, it's written at write_ptr and write_ptr is increased by
 * the number of bytes written.
 *
 * When data is read, it's read at read_ptr and read_ptr advances by the number of bytes
 * consumed.
 *
 * Proper checks are put in place to ensure that write_ptr doesn't write outside the
 * buffer boundaries and that read_ptr never exceeds write_ptr.
 *
 * Of course, after some data has been written to the buffer, there might not be enough
 * space remaining in the buffer to store another chunk of data.  However, as read_ptr
 * advances, space is freed at the beginning of the buffer until finally, read_ptr
 * reaches the same value as write_ptr.  At this point no more data is waiting to be
 * read.  It's now safe to reset write_ptr and read_ptr to point at the beginning of
 * the buffer again.  The full size of the buffer is available for storing data again.
 *
 * Of course, the consequence of this approach is that the write_ptr can only be reset
 * when all data stored in the buffer is consumed.  This blocks the core receiving the
 * data unnecessarily, because the total amount of free bytes in the buffer might
 * actually be sufficient to hold the next chunk of data to be written.
 *
 * However, if data is in general consumed quickly, this approach can be more efficient
 * than more sophisticated buffer management strategies.
 *
 * Moreover, to avoid blocking, the buffer content may need to be rearranged using
 * memcpy or memmove, which can also take longer than simply waiting.
 *
 * On top of that, the implementation is simple and easy to understand, and therefore
 * less likely to contain bugs.
 */
class SingleFrameProxy: public Proxy {
    public:
        SingleFrameProxy(unsigned long timeout_ms = 3000, esp_err_t error_on_no_memory = ESP_ERR_NO_MEM): timeout(timeout_ms),
            error_on_no_memory(error_on_no_memory), buffer(nullptr), buffer_size(0), read_ptr(nullptr), frame_size(0) {}

        SingleFrameProxy(const SingleFrameProxy & other) = delete;
        const SingleFrameProxy & operator=(const SingleFrameProxy & other) = delete;

        ~SingleFrameProxy() { free(buffer); }

        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) override {
            std::unique_lock<std::mutex> lock(recv_mutex);
            if (!cond.wait_for(
                        lock,
                        timeout,
            [this]() -> bool {
            return !read_ptr;
        })) {
                // no space left in buffer
                return error_on_no_memory;
            }

            if (buffer_size < frame->len) {
                // buffer too small for frame
                char * new_buffer = (char *) realloc(buffer, frame->len);
                if (!new_buffer) {
                    // not enough memory to extend buffer
                    return error_on_no_memory;
                }
                // buffer resized
                buffer = new_buffer;
                buffer_size = frame->len;
            }

            frame->payload = (uint8_t *)(buffer);
            esp_err_t ret = httpd_ws_recv_frame(request, frame, frame->len);
            if (ret != ESP_OK) {
                ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
            } else {
                read_ptr = buffer;
                frame_size = frame->len;
            }

            return ret;
        }

        virtual int available() override {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return read_ptr ? (buffer + frame_size - read_ptr) : 0;
        }

        virtual int read(uint8_t * ptr, size_t len) {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            const size_t bytes_available = buffer + frame_size - read_ptr;
            const size_t bytes_to_read = len < bytes_available ? len : bytes_available;
            memcpy(ptr, read_ptr, bytes_to_read);
            read_ptr += bytes_to_read;
            if (read_ptr >= buffer + frame_size) {
                // all queued data consumed
                read_ptr = nullptr;
                frame_size = 0;
                cond.notify_all();
            }
            return bytes_to_read;
        }

        virtual int peek() override {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return read_ptr ? ((unsigned char *) read_ptr)[0] : -1;
        }

        const std::chrono::milliseconds timeout;
        const esp_err_t error_on_no_memory;

    protected:
        std::mutex recv_mutex;
        std::condition_variable cond;

        char * buffer;
        size_t buffer_size;

        char * read_ptr;
        size_t frame_size;
};

}
