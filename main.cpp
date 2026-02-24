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

    vector<thread> threads;

    for (int i = 0; i < NUM_OF_PARTICIPANTS; ++i) {
        Participant temp_user(rand(),MODULO, (i + 1), bullet);
        thread temp(bind(&Participant::startServer, &temp_user));
        threads.push_back(temp);
    }

    //\\//\\//\\//\\//\\//\\//\\
    //  2. SECRET GENERATION  \\
    //\\//\\//\\//\\//\\//\\//\\
    
    //cout << "Together's (A,B,C) secret: " << Polynomial::lagrange({ {Alice.id, final_numbers[0]}, {Bob.id, final_numbers[1]}, {Carol.id, final_numbers[2]} }, MODULO) << endl;


    //\\//\\//\\//\\//\\//\\//\\
    //   3.SHARE GENERATION   \\
    //\\//\\//\\//\\//\\//\\//\\

    Participant temp_user2(0, MODULO, (NUM_OF_PARTICIPANTS+1), bullet);
    thread t1(bind(&Participant::startServer, &temp_user2));
    threads.push_back(t1);

    // TODO : Chose random leader by Bulletin

    int leader = rand() % NUM_OF_PARTICIPANTS;


    /*
    // PoC
    cout << "Together's (B,C,D) secret: " << Polynomial::lagrange({ {Bob.id, final_numbers[1]}, {Carol.id, final_numbers[2]}, { Dave.id, new_piece} }, MODULO) << endl;

    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(totals[0], Alice.getSk()) << " bits" << endl;
    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(totals[1], Bob.getSk()) << " bits" << endl;
    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(totals[2], Carol.getSk()) << " bits" << endl;
    cout << "Remaining Noise Budget: " << bullet.E.getNoiseBudget(dave_share, Dave.getSk()) << " bits" << endl;
    */

    for (int i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    return 0;
}
