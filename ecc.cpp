/*

#include <vector>
#include <string>
#include <iostream>
#include "ecdsa.hpp" 

int main() {
    auto [publicKey, privateKey] = ECDH::generateKeyPair();

    std::string message = "Hello, world!";

    std::vector<unsigned char> data(message.begin(), message.end());

    auto signature = ECDSA::sign(privateKey, data);
    bool isValid = ECDSA::verify(publicKey, data, signature);

    if (isValid) {
        std::cout << "Signature is valid." << std::endl;
    }
    else {
        std::cout << "Signature is invalid." << std::endl;
    }

    return 0;
}
*/