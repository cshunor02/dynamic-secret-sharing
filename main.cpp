#include <stdio.h>
#include <iostream>
#include "participant.cpp"

using namespace std;

int main() {
    Participant Alice(32);
    vector<int> alice_pol = Alice.getPolynomial();

    Alice.generateRandomPolynomial(3);

    alice_pol = Alice.getPolynomial();

    for (int i : alice_pol)
        std::cout << i << ' ';


    return 0;
}
