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
class StaticBufferProxy: public Proxy {
    public:
        StaticBufferProxy(const size_t size = 1024, unsigned long timeout_ms = 3000,
                          esp_err_t error_on_no_memory = ESP_ERR_NO_MEM):
            size(size), timeout(timeout_ms), error_on_no_memory(error_on_no_memory),
            buffer((char *) malloc(size)), read_ptr(buffer), write_ptr(buffer) {}

        virtual ~StaticBufferProxy() { free(buffer); }

        virtual size_t get_space_available_for_write() {
            const size_t space_tail = (buffer + size) - write_ptr;
            return space_tail;
        }

        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) override {
            const size_t frame_size = frame->len;

            std::unique_lock<std::mutex> lock(recv_mutex);
            if (!cond.wait_for(
                        lock,
                        timeout,
            [this, frame_size]() -> bool {
            const size_t space_tail = (buffer + size) - write_ptr;
                return frame_size <= space_tail;
            })) {
                // no space left in buffer
                return error_on_no_memory;
            }

            return receive_data(request, frame);
        }

        virtual int available() override {
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
                if (write_ptr == read_ptr) {
                    // read_ptr reached write_ptr.  The buffer is empty.
                    // reset the read and write pointers to get the most
                    // space for the next recv
                    read_ptr = buffer;
                    write_ptr = buffer;
                }
                cond.notify_all();
            }
            return bytes_to_read;
        }

        virtual int peek() override {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            return (write_ptr != read_ptr) ? ((unsigned char *) read_ptr)[0] : -1;
        }

        const size_t size;
        const std::chrono::milliseconds timeout;
        const esp_err_t error_on_no_memory;

    protected:
        /* Read received data into the buffer (at write_ptr) */
        esp_err_t receive_data(httpd_req_t * request, httpd_ws_frame_t * frame) {
            const size_t frame_size = frame->len;
            frame->payload = (uint8_t *)(write_ptr);
            esp_err_t ret = httpd_ws_recv_frame(request, frame, frame_size);
            if (ret != ESP_OK) {
                ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
            } else {
                write_ptr += frame_size;
            }
            return ret;
        }

        std::mutex recv_mutex;
        std::condition_variable cond;

        char * buffer;
        char * read_ptr;
        char * write_ptr;
};

}
