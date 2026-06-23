#pragma once

#include <stdexcept>
#include <string>

extern "C" {
#include "def.h"
}

namespace idrto {

class DefError : public std::runtime_error {
public:
    explicit DefError(def_status status)
        : std::runtime_error(message_for(status)), status_(status) {}

    def_status status() const { return status_; }

private:
    def_status status_;

    static const char *message_for(def_status status) {
        switch (status) {
        case DEF_ERR_LABEL_TOO_LONG:
            return "encoded label exceeds 63 characters";
        case DEF_ERR_INVALID_ESCAPE:
            return "invalid escape sequence";
        case DEF_ERR_INVALID_UTF8:
            return "invalid utf-8 byte sequence";
        case DEF_ERR_BUFFER_TOO_SMALL:
            return "output buffer too small";
        default:
            return "unknown def error";
        }
    }
};

inline std::string encode(const std::string &input) {
    char buffer[128];
    size_t length = 0;
    def_status status = def_encode(input.c_str(), buffer, sizeof(buffer), &length);
    if (status != DEF_OK) {
        throw DefError(status);
    }
    return std::string(buffer, length);
}

inline std::string decode(const std::string &encoded) {
    char buffer[128];
    size_t length = 0;
    def_status status = def_decode(encoded.c_str(), buffer, sizeof(buffer), &length);
    if (status != DEF_OK) {
        throw DefError(status);
    }
    return std::string(buffer, length);
}

}  // namespace idrto
