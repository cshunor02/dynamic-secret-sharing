#include <stdio.h>
#include <iostream>
#include <ctime>
#include "participant.cpp"

using namespace std;

#define MODULO 1024
#define NUM_OF_PARTICIPANTS 3
#define t 3

int main() {
    srand(time(0));
    //\\//\\//\\//\\
    //  1. SETUP  \\
    //\\//\\//\\//\\

    Bulletin bullet;
    bullet.initialize(4096, MODULO); // the secrecy of E, modulo

    // Participant(secret, modulo, id)
    vector<Participant> parties;
    Participant Alice(32, MODULO, 1);
    Participant Bob(27, MODULO, 2);
    Participant Carol(3, MODULO, 3);

    // TODO: Check whether id's are unique

    //\\//\\//\\//\\//\\//\\//\\
    //  2. SECRET GENERATION  \\
    //\\//\\//\\//\\//\\//\\//\\

    // For each i, Pi generates a pair of key pki, ski
    Alice.getKeyPairs(bullet);
    Bob.getKeyPairs(bullet);
    Carol.getKeyPairs(bullet);
    
    // Participants generate p(x) polynomials at degree t
    Alice.startPolynomialCalculation(t);
    Bob.startPolynomialCalculation(t);
    Carol.startPolynomialCalculation(t);

    parties.push_back(Alice);
    parties.push_back(Bob);
    parties.push_back(Carol);

    for (int i = 0; i < NUM_OF_PARTICIPANTS; i++) {
        vector<Ciphertext> collect;
        for (int j = 0; j < NUM_OF_PARTICIPANTS; j++) {
            int temp = parties[i].getPointValue(parties[j].id);

            collect.push_back(bullet.E.encryptSecret(bullet.public_keys[j], temp));
        }
        bullet.shares.push_back(collect);
    }

    vector<Ciphertext> totals;
    for (int i = 0; i < t; i++) {
        totals.push_back(bullet.E.sumShares({ bullet.shares[0][i], bullet.shares[1][i], bullet.shares[2][i] }));
    }

    vector<int> final_numbers;
    int temp;
    for (int i = 0; i < t; i++) {
        bullet.E.decryptSecret(parties[i].getSk(), totals[i], temp);
        final_numbers.push_back(temp);
        cout << (i+1) << "th share: " << temp << endl;
    }

    //cout << "Together's secret: " << Polynomial::lagrange({ {Alice.id, final_numbers[0]}, {Bob.id, final_numbers[1]}, {Carol.id, final_numbers[2]} }, MODULO) << endl;

    //\\//\\//\\//\\//\\//\\//\\
    //   3.SHARE GENERATION   \\
    //\\//\\//\\//\\//\\//\\//\\

    Participant Dave(0, MODULO, 4);
    Dave.getKeyPairs(bullet);

    // Choose random leader

    int leader = rand() % NUM_OF_PARTICIPANTS;


    return 0;
}
