#include <stdio.h>
#include <iostream>
#include "participant.cpp"

using namespace std;

#define MODULO 10
#define NUM_OF_PARTICIPANTS 2

int main() {
    Participant Alice(32, MODULO, 101);
    Participant Bob(27, MODULO, 102);

    Bulletin bullet;
    bullet.initialize(1024);

    Alice.startPolynomialCalculation(NUM_OF_PARTICIPANTS);
    Alice.getKeyPairs(bullet);

    Bob.startPolynomialCalculation(NUM_OF_PARTICIPANTS);
    Bob.getKeyPairs(bullet);

    int resA1 = Alice.getPointValue(1); //Alice's secret piece
    int resA2 = Alice.getPointValue(2); //Bob's secret piece

    int resB1 = Bob.getPointValue(1); //Alice's secret piece
    int resB2 = Bob.getPointValue(2); //Bob's secret piece

    cout << "resA1 " << resA1 << endl;
    cout << "resA2 " << resA2 << endl;
    cout << "resB1 " << resB1 << endl;
    cout << "resB2 " << resB2 << endl;

    Ciphertext cipA1 = bullet.E.encryptSecret(bullet.public_keys[0], resA1);
    Ciphertext cipB1 = bullet.E.encryptSecret(bullet.public_keys[1], resA2);
    Ciphertext cipA2 = bullet.E.encryptSecret(bullet.public_keys[0], resB1);
    Ciphertext cipB2 = bullet.E.encryptSecret(bullet.public_keys[1], resB2);

    Ciphertext res;
    string ress;

    // Alice adds up A1 and B1
    bullet.E.addSecrets(cipA1, cipA2, res);

    bullet.E.decryptSecret(Alice.getSk(), res, ress);
    cout << ress << endl;

    // Bob adds together A2 and B2 
    bullet.E.addSecrets(cipB1, cipB2, res);

    bullet.E.decryptSecret(Bob.getSk(), res, ress);
    cout << ress << endl;

    return 0;
}
