#include <stdio.h>
#include <iostream>
#include <ctime>
#include "participant.cpp"

#include <future>
#include <string>
#include <thread>
#include <chrono>

#include "zmq.hpp"
#include "zmq_addon.hpp"

using namespace std;
using namespace zmq;

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
    Participant Alice(32, MODULO, 1, bullet);
    Participant Bob(27, MODULO, 2, bullet);
    Participant Carol(3, MODULO, 3, bullet);

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

    parties.reserve(20);

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
        cout << (i + 1) << "th share: " << temp << endl;
    }

    cout << "Together's (A,B,C) secret: " << Polynomial::lagrange({ {Alice.id, final_numbers[0]}, {Bob.id, final_numbers[1]}, {Carol.id, final_numbers[2]} }, MODULO) << endl;

    //\\//\\//\\//\\//\\//\\//\\
    //   3.SHARE GENERATION   \\
    //\\//\\//\\//\\//\\//\\//\\

    Participant Dave(0, MODULO, 4, bullet);
    Dave.getKeyPairs(bullet);

    // Choose random leader

    int leader = rand() % NUM_OF_PARTICIPANTS;

    vector<Ciphertext> new_shares;

    vector<int> random_masks;

    for (int i = 0; i < NUM_OF_PARTICIPANTS; i++) {
        int mask = rand();
        random_masks.push_back(mask);
        new_shares.push_back(
            parties[i].newUserJoin(
                Dave.id,
                *(bullet.public_keys.end() - 1),
                final_numbers[i],
                { {1, 0}, {2, 0}, {3, 0} },
                bullet,
                mask)
        );
    }

    int sum_mask = 0;
    for (int i = 0; i < random_masks.size(); i++) {
        sum_mask += random_masks[i];
    }
    sum_mask *= -1;
    sum_mask %= MODULO;

    while (sum_mask < 0) {
        sum_mask += MODULO;
    }

    Ciphertext result = bullet.E.sumShares(new_shares); // Leader does this
    Ciphertext mask_enc = bullet.E.encryptSecret(*(bullet.public_keys.end() - 1), sum_mask);

    Ciphertext dave_share;

    bullet.E.addShares(result, mask_enc, dave_share);

    int new_piece;
    bullet.E.decryptSecret(Dave.getSk(), dave_share, new_piece);

    cout << "Dave's new share: " << new_piece << endl;

    // PoC
    cout << "Together's (B,C,D) secret: " << Polynomial::lagrange({ {Bob.id, final_numbers[1]}, {Carol.id, final_numbers[2]}, { Dave.id, new_piece} }, MODULO) << endl;

    /*
    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(totals[0], Alice.getSk()) << " bits" << endl;
    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(totals[1], Bob.getSk()) << " bits" << endl;
    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(totals[2], Carol.getSk()) << " bits" << endl;
    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(dave_share, Dave.getSk()) << " bits" << endl;
    */

    auto thread1 = async(launch::async, &Participant::startServer, &Alice);

    this_thread::sleep_for(chrono::milliseconds(500));

    auto thread2 = async(launch::async, &Participant::startServer, &Bob);
    this_thread::sleep_for(chrono::milliseconds(500));

    if (!Alice.location.empty()) {
        Bob.sendMessage(Alice.location, bullet.shares[0][0]);
    }

    this_thread::sleep_for(chrono::seconds(1));

    Alice.stopProcess = true;
    Bob.stopProcess = true;
    thread1.wait();
    thread2.wait();

    return 0;
}
