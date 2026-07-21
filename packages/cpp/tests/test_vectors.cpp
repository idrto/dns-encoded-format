#include <iostream>
#include <string>

#include "idrto/def.hpp"

int main() {
    if (idrto::encode("alice") != "dalice") {
        std::cerr << "encode alice failed\n";
        return 1;
    }

    const auto hashEncoded = idrto::encode(std::string(64, 'a'));
    if (hashEncoded.empty() || hashEncoded[0] != 'h') {
        std::cerr << "expected hash encoding for long input\n";
        return 1;
    }

    if (idrto::encode("user@example.com/db1.us-east/accounts-db") !=
        "duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb") {
        std::cerr << "encode path example failed\n";
        return 1;
    }

    if (idrto::decode("duser-40example-2ecom-2fdb1-2eus-2deast-2faccounts-2ddb") !=
        "user@example.com/db1.us-east/accounts-db") {
        std::cerr << "decode path example failed\n";
        return 1;
    }

    if (idrto::decode("duser-40example-2ecom") != "user@example.com") {
        std::cerr << "decode failed\n";
        return 1;
    }

    try {
        idrto::decode("h34dk0ez8tm7vyf659gc3tm7tfyv1n4fz6iqhx4wv7dte31ztx0");
        std::cerr << "expected not_decodable on hash decode\n";
        return 1;
    } catch (const idrto::DefError &err) {
        if (err.status() != DEF_ERR_NOT_DECODABLE) {
            std::cerr << "unexpected decode error\n";
            return 1;
        }
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
