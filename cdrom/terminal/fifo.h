#pragma once
#include <stddef.h>

template <typename T, size_t length>
class fifo {
    // Elements are added to the back and are read from the front
    T data[length] = {};
    size_t write_ptr = 0;
    size_t read_ptr = 0;
    bool full = false;

   public:
    size_t size() const {
        if (write_ptr >= read_ptr) {
            return write_ptr - read_ptr;
        } else {
            return length + write_ptr - read_ptr;
        }
    }

    bool is_empty() const { return write_ptr == read_ptr && !full; }

    bool is_full() const { return full; }

    void clear() {
        write_ptr = 0;
        read_ptr = 0;
        full = false;
    }

    bool add(const T t) {
        if (is_full()) {
            return false;
        }

        data[write_ptr] = t;
        write_ptr = (write_ptr + 1) % length;

        full = write_ptr == read_ptr;

        return true;
    }

    T get() {
        if (is_empty()) {
            return 0;
        }

        T t = data[read_ptr];
        read_ptr = (read_ptr + 1) % length;

        full = false;

        return t;
    }

    T peek(const size_t ptr = 0) const {
        if (ptr >= size()) {
            return 0;
        }

        return data[(read_ptr + ptr) % length];
    }

    T operator[](size_t ptr) const { return peek(ptr); }
};