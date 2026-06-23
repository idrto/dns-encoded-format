#include <iostream>
#include <string>

#include "idrto/def.hpp"

int main() {
    if (idrto::encode("alice") != "alice") {
        std::cerr << "encode alice failed\n";
        return 1;
    }

    try {
        idrto::encode(std::string(64, 'a'));
        std::cerr << "expected label_too_long on encode\n";
        return 1;
    } catch (const idrto::DefError &err) {
        if (err.status() != DEF_ERR_LABEL_TOO_LONG) {
            std::cerr << "unexpected encode error\n";
            return 1;
        }
    }

    if (idrto::encode("user@example.com/db1.us-east/accounts-db") !=
        "user-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb") {
        std::cerr << "encode path example failed\n";
        return 1;
    }

    if (idrto::decode("user-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb") !=
        "user@example.com/db1.us-east/accounts-db") {
        std::cerr << "decode path example failed\n";
        return 1;
    }

    if (idrto::decode("user-40example-2ecom") != "user@example.com") {
        std::cerr << "decode failed\n";
        return 1;
    }

    try {
        idrto::decode(std::string(64, 'a'));
        std::cerr << "expected label_too_long on decode\n";
        return 1;
    } catch (const idrto::DefError &err) {
        if (err.status() != DEF_ERR_LABEL_TOO_LONG) {
            std::cerr << "unexpected decode error\n";
            return 1;
        }
    }

    return 0;
}
