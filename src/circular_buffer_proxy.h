#pragma once

#include <Arduino.h>

#include "shifting_buffer_proxy.h"

namespace PsychicWebSocketProxy {

/* This class implements a circular buffer for storing received data.  However, this is not a typical
 * circular buffer, becauese of the limitations of the ESP32 websocket implementation.  The ESP32 API
 * only allows receiving each Websocket protocol frame in one go, into one continuous memory range.
 * For this reason, if there is enough free space in the buffer, but not in a continous chunk, the
 * contents of the buffer are moved to ensure the free space is continuous and at the end of the buffer.
 *
 * The implementation of this class uses ShiftingBufferProxy as a base, because some of it's methods
 * can be reused.
 */
template <size_t size, unsigned long timeout_ms = 3000, esp_err_t error_on_no_memory = ESP_ERR_NO_MEM>
class CircularBufferProxy: public ShiftingBufferProxy<size, timeout_ms, error_on_no_memory> {
    public:
        virtual esp_err_t recv(httpd_req_t * request, httpd_ws_frame_t * frame) override {
            const size_t frame_size = frame->len;

            std::unique_lock<std::mutex> lock(recv_mutex);

            if (!cond.wait_for(
                        lock,
                        std::chrono::milliseconds(timeout_ms),
            [this, frame_size]() -> bool {
            if (read_ptr <= write_ptr) {
                    const size_t space_tail = (buffer + size) - write_ptr;
                    const size_t space_head = read_ptr - buffer;
                    const size_t space_total = space_tail + space_head - 1;
                    return frame_size <= space_total;
                } else {
                    const size_t space_middle = read_ptr - read_ptr;
                    const size_t space_tail = (buffer + size) - read_wrap;
                    const size_t space_total = space_middle + space_tail - 1;
                    return frame_size <= space_total;
                }
            })) {
                // no space left in buffer
                return error_on_no_memory;
            }

            if (read_ptr <= write_ptr) {
                /*
                    |.....#####.....|
                    ^     ^    ^
                    |     |    +- write_ptr
                    |     +------ read_ptr
                    +------------ buffer
                */
                const size_t space_tail = (buffer + size) - write_ptr;
                const size_t space_head = read_ptr - buffer - 1;

                if (space_tail >= frame_size) {
                    // sweet! frame will fit into the remaining space in the buffer
                    return receive_data(request, frame);
                } else if (space_head >= frame_size) {
                    // frame will not fit into the free space at the end of the buffer,
                    // but it can go into the head
                    if (read_ptr == write_ptr) {
                        // there's no data waiting in the buffer, wrap read_ptr right away
                        read_ptr = buffer;
                    } else {
                        // there's some data still waiting to be read, remember to wrap read_ptr at the right point
                        read_wrap = write_ptr;
                    }
                    write_ptr = buffer;
                    return receive_data(request, frame);
                } else {
                    // we have enough space, but not in a continuous chunk, let's fix that
                    shift_buffer();
                    return receive_data(request, frame);
                }

            } else {
                /*
                    |#####.....###..|
                    ^     ^    ^  ^
                    |     |    |  +- read_wrap
                    |     |    +---- read_ptr
                    |     +--------- write_ptr
                    +--------------- buffer
                */

                const size_t space_middle = read_ptr - read_ptr;

                if (space_middle < frame_size) {
                    // not enough space in the middle, shift tail to the end of the buffer
                    shift_buffer_tail();
                }
                return receive_data(request, frame);
            }

        }

        virtual int available() {
            const std::lock_guard<std::mutex> lock(recv_mutex);
            if (read_ptr <= write_ptr) {
                return write_ptr - read_ptr;
            } else {
                return (read_wrap - read_ptr) + (write_ptr - buffer);
            }
        }

        virtual int read(uint8_t * ptr, size_t len) {
            const std::lock_guard<std::mutex> lock(recv_mutex);

            uint8_t * dst_ptr = ptr;

            if (read_ptr > write_ptr) {
                const size_t bytes_available = read_wrap - read_ptr;
                const size_t bytes_to_read = len < bytes_available ? len : bytes_available;

                memcpy(dst_ptr, read_ptr, bytes_to_read);
                read_ptr += bytes_to_read;
                dst_ptr += bytes_to_read;

                if (read_ptr >= read_wrap) {
                    // wrap point reached
                    read_ptr = buffer;
                    // read_wrap = 0;
                }

                len -= bytes_to_read;
            }

            if (len) {
                const size_t bytes_available = write_ptr - read_ptr;
                const size_t bytes_to_read = len < bytes_available ? len : bytes_available;
                memcpy(dst_ptr, read_ptr, bytes_to_read);
                read_ptr += bytes_to_read;
                dst_ptr += bytes_to_read;
                // len -= bytes_to_read;
            }

            cond.notify_all();

            return dst_ptr - ptr;
        }

    protected:
        /* Move the unread contents of the buffer to free up space in the middle.
            before:
                |#####.....###..|
                ^     ^    ^  ^
                |     |    |  +- read_wrap
                |     |    +---- read_ptr
                |     +--------- write_ptr
                +--------------- buffer
            after:
                |#####.......###|
                ^     ^      ^  ^
                |     |      |  +- read_wrap
                |     |      +---- read_ptr
                |     +----------- write_ptr
                +----------------- buffer
        */
        void shift_buffer_tail() {
            const size_t shift_size = buffer + size - read_wrap;
            memmove(read_ptr + shift_size, read_ptr, shift_size);
            read_ptr += shift_size;
            read_wrap = buffer + size;
        }

        char * read_wrap;

        using ShiftingBufferProxy<size, timeout_ms, error_on_no_memory>::receive_data;
        using ShiftingBufferProxy<size, timeout_ms, error_on_no_memory>::shift_buffer;
        using ShiftingBufferProxy<size, timeout_ms, error_on_no_memory>::recv_mutex;
        using ShiftingBufferProxy<size, timeout_ms, error_on_no_memory>::cond;
        using ShiftingBufferProxy<size, timeout_ms, error_on_no_memory>::buffer;
        using ShiftingBufferProxy<size, timeout_ms, error_on_no_memory>::read_ptr;
        using ShiftingBufferProxy<size, timeout_ms, error_on_no_memory>::write_ptr;
};

}
