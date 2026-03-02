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
    //\\//\\//\\//\\
    //  1. SETUP  \\
    //\\//\\//\\//\\

    srand(time(0));
    Bulletin bullet;
    bullet.initialize(4096, MODULO, t); // the secrecy of E, modulo

    vector<Participant*> participants;

    for (int i = 0; i < NUM_OF_PARTICIPANTS; ++i) {
        int secret = rand() % MODULO;
        int id = i + 1;

        Participant* p = new Participant(secret, MODULO, id, bullet);
        p->location = "tcp://127.0.0.1:" + to_string(5550 + id); // 5551, 5552, 5553

        participants.push_back(p);

        bullet.ids.push_back(id);
        bullet.destinations.push_back(p->location);
        bullet.points.push_back({ id, 0 });
    }

    //\\//\\//\\//\\//\\//\\//\\
    //  2. SECRET GENERATION  \\
    //\\//\\//\\//\\//\\//\\//\\
    
    for (auto p : participants) {
        p->startSecretGeneration();
    }

    //\\//\\//\\//\\//\\//\\//\\
    //   3.SHARE GENERATION   \\
    //\\//\\//\\//\\//\\//\\//\\

    Participant* new_user = new Participant(0, MODULO, NUM_OF_PARTICIPANTS + 1, bullet);
    new_user->location = "tcp://127.0.0.1:5560";

    new_user->getKeyPairs(bullet);
    bullet.ids.push_back(new_user->id);
    bullet.destinations.push_back(new_user->location);

    // TODO : Chose random leader by Bulletin

    bullet.leaderId = rand() % NUM_OF_PARTICIPANTS;
    
    participants[bullet.leaderId]->leader = true;

    vector<thread> threads;
    for (auto p : participants) {
        threads.push_back(thread(&Participant::startServer, p));
    }
    threads.push_back(thread(&Participant::startServer, new_user));

    this_thread::sleep_for(chrono::milliseconds(500));

    vector<thread> worker_threads;
    for (auto p : participants) {
        worker_threads.push_back(thread(&Participant::startShareGeneration, p));
    }

    // Megvárjuk, amíg mindenki befejezi az üzenetküldést és összegzést
    for (auto& x : worker_threads) {
        x.join();
    }

    this_thread::sleep_for(chrono::seconds(1));

    for (auto p : participants) p->stopReceiving = true;
    new_user->stopReceiving = true;

    // Megvárjuk, míg a szerverszálak leállnak
    for (auto& x : threads) x.join();

    // Memória felszabadítás
    for (auto p : participants) delete p;
    delete new_user;

    return 0;
}
