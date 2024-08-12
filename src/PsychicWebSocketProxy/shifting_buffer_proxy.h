#pragma once

#include <Arduino.h>

#include "static_buffer_proxy.h"

namespace PsychicWebSocketProxy {

/* This proxy implementation is similar to the StaticBufferProxy, but it doesn't
 * block waiting for the reader to consume all data before receiving a frame, which
 * wouldn't fit in the tail of the buffer.
 *
 * Instead, it checks if there's enough total space in the buffer to fit the buffer
 * considering both free space at the beginning and at the end.  If there's not enough
 * space at the end, but total space is enough to store the next chunk of data, it
 * shifts the unread buffer contents to the beginning of the buffer, making continuous
 * space available at the end.
 */
class ShiftingBufferProxy: public StaticBufferProxy {
    public:
        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) override {
            const size_t frame_size = frame->len;

            std::unique_lock<std::mutex> lock(recv_mutex);
            if (!cond.wait_for(
                        lock,
                        timeout,
            [this, frame_size]() -> bool {
            const size_t space_total = size - (write_ptr - read_ptr);
                return frame_size <= space_total;
            })) {
                // no space left in buffer
                return error_on_no_memory;
            }

            const size_t space_tail = (buffer + size) - write_ptr;

            if (space_tail < frame_size) {
                // there's not enough memory at the end of the buffer, but
                // we can recover some memory at the beginning
                const size_t move_size = write_ptr - read_ptr;
                const size_t shift_size = read_ptr - buffer;
                memmove(buffer, read_ptr, move_size);
                write_ptr -= shift_size;
                read_ptr -= shift_size;
            }

            return receive_data(request, frame);
        }

    protected:
        /* Move the unread contents of the buffer to free up space at the end.
            before:
                |.....#####.....|
                ^     ^    ^
                |     |    +- write_ptr
                |     +------ read_ptr
                +------------ buffer
            after:
                |#####..........|
                ^     ^
                |     +- write_ptr
                +- buffer = read_ptr
        */
        void shift_buffer() {
            const size_t move_size = write_ptr - read_ptr;
            const size_t shift_size = read_ptr - buffer;
            memmove(buffer, read_ptr, move_size);
            write_ptr -= shift_size;
            read_ptr -= shift_size;
        }
};

}
