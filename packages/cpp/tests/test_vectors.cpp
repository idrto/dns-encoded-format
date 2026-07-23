#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "idrto/def.hpp"

namespace {

std::string read_file(const std::string &path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string extract_string(const std::string &obj, const std::string &key) {
    const std::string needle = "\"" + key + "\": \"";
    const auto pos = obj.find(needle);
    if (pos == std::string::npos) {
        return "";
    }
    const auto start = pos + needle.size();
    const auto end = obj.find('"', start);
    return obj.substr(start, end - start);
}

std::string extract_reason(const std::string &obj) {
    return extract_string(obj, "reason");
}

def_status reason_to_status(const std::string &reason) {
    if (reason == "label_too_long") return DEF_ERR_LABEL_TOO_LONG;
    if (reason == "invalid_escape") return DEF_ERR_INVALID_ESCAPE;
    if (reason == "invalid_utf8") return DEF_ERR_INVALID_UTF8;
    if (reason == "invalid_encoding") return DEF_ERR_INVALID_ENCODING;
    if (reason == "invalid_locator") return DEF_ERR_INVALID_LOCATOR;
    if (reason == "not_decodable") return DEF_ERR_NOT_DECODABLE;
    return DEF_ERR_INVALID_ENCODING;
}

bool run_section(const std::string &json, const std::string &section) {
    const std::string key = "\"" + section + "\": [";
    const auto start = json.find(key);
    if (start == std::string::npos) {
        std::cerr << "missing section " << section << "\n";
        return false;
    }

    auto pos = start + key.size();
    const auto end = json.find(']', pos);
    const std::string body = json.substr(pos, end - pos);

    size_t cursor = 0;
    while ((cursor = body.find('{', cursor)) != std::string::npos) {
        const auto close = body.find('}', cursor);
        const std::string obj = body.substr(cursor, close - cursor + 1);

        if (section.rfind("encode", 0) == 0 && section.find("_errors") == std::string::npos) {
            const auto input = extract_string(obj, "input");
            const auto encoded = extract_string(obj, "encoded");
            if (section == "encode_body") {
                if (idrto::encode_body(input) != encoded) {
                    std::cerr << section << " failed for " << input << "\n";
                    return false;
                }
            } else {
                if (idrto::encode_profile(input) != encoded) {
                    std::cerr << section << " failed for " << input << "\n";
                    return false;
                }
            }
        } else if (section.rfind("decode", 0) == 0 && section.find("_errors") == std::string::npos) {
            const auto input = extract_string(obj, "input");
            const auto decoded = extract_string(obj, "decoded");
            if (section == "decode_body") {
                if (idrto::decode_body(input) != decoded) {
                    std::cerr << section << " failed for " << input << "\n";
                    return false;
                }
            } else {
                if (idrto::decode_profile(input) != decoded) {
                    std::cerr << section << " failed for " << input << "\n";
                    return false;
                }
            }
        } else if (section.find("_errors") != std::string::npos) {
            const auto input = extract_string(obj, "input");
            const auto reason = extract_reason(obj);
            try {
                if (section == "decode_body_errors") {
                    idrto::decode_body(input);
                } else if (section == "decode_profile_errors") {
                    idrto::decode_profile(input);
                } else if (section == "encode_profile_errors") {
                    idrto::encode_profile(input);
                }
                std::cerr << section << " expected error for " << input << "\n";
                return false;
            } catch (const idrto::DefError &err) {
                if (err.status() != reason_to_status(reason)) {
                    std::cerr << section << " wrong error for " << input << "\n";
                    return false;
                }
            }
        }

        cursor = close + 1;
    }

    return true;
}

}  // namespace

int main() {
    const auto json = read_file(TEST_VECTORS_PATH);
    const char *sections[] = {
        "encode_body",
        "decode_body",
        "decode_body_errors",
        "encode_profile",
        "encode_profile_hash",
        "encode_profile_errors",
        "decode_profile",
        "decode_profile_errors",
    };

    for (const char *section : sections) {
        if (!run_section(json, section)) {
            return 1;
        }
    }

    return 0;
}
