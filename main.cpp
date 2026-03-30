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
using namespace chrono;

int main(int argc, char* argv[]) {
    //\\//\\//\\//\\
    //  1. SETUP  \\
    //\\//\\//\\//\\

    int NUM_OF_PARTICIPANTS = 10;
    int t = 2;
    int MODULO = 1021;

    if (argc >= 3) {
        NUM_OF_PARTICIPANTS = stoi(argv[1]);
        t = stoi(argv[2]);
        MODULO = stoi(argv[3]);
    }

    srand(time(0));
    Bulletin bullet;

    auto setup_start = high_resolution_clock::now();
    bullet.initialize(4096, MODULO, t); // the secrecy of E, modulo

    vector<Participant*> participants;

    for (int i = 0; i < t; ++i) {
        int secret = rand() % MODULO;
        int id = i + 1;

        Participant* p = new Participant(secret, MODULO, id, bullet);
        p->location = "tcp://127.0.0.1:" + to_string(5550 + id);
        participants.push_back(p);

        bullet.ids.push_back(id);
        bullet.destinations.push_back(p->location);
        bullet.points.push_back({ id, 0 });
    }
    auto setup_stop = high_resolution_clock::now();

    //\\//\\//\\//\\//\\//\\//\\
    //  2. SECRET GENERATION  \\
    //\\//\\//\\//\\//\\//\\//\\
    
    auto sec_gen_start = high_resolution_clock::now();
    for (auto p : participants) {
        p->startSecretGeneration();
    }
    auto sec_gen_stop = high_resolution_clock::now();

    //\\//\\//\\//\\//\\//\\//\\
    //   3.SHARE GENERATION   \\
    //\\//\\//\\//\\//\\//\\//\\

    bullet.leaderId = rand() % t;
    participants[bullet.leaderId]->leader = true;
    vector<thread> threads;

    for (auto p : participants) {
        threads.push_back(thread(&Participant::startServer, p));
    }

    this_thread::sleep_for(milliseconds(500));

    long long total_share_gen_time = 0;

    for (int i = 0; i < (NUM_OF_PARTICIPANTS - t); ++i) {
        vector<thread> workerThreads;
        Participant* p = new Participant(0, MODULO, t + 1 + i, bullet);
        p->location = "tcp://127.0.0.1:" + to_string(5550 + t + 1 + i);

        p->getKeyPairs(bullet);
        participants.push_back(p);
        bullet.ids.push_back(p->id);
        bullet.destinations.push_back(p->location);
        bullet.points.push_back({ t + 1 + i, 0 });

        threads.push_back(thread(&Participant::startServer, p));
        this_thread::sleep_for(milliseconds(100));

        auto single_share_start = high_resolution_clock::now();
        
        for (int j = 0; j < t; ++j) {
            workerThreads.push_back(thread(&Participant::startShareGeneration, participants[j]));
        }

        for (auto& x : workerThreads) {
            x.join();
        }

        auto single_share_stop = high_resolution_clock::now();
        total_share_gen_time += duration_cast<milliseconds>(single_share_stop - single_share_start).count();
    }

    this_thread::sleep_for(seconds(1));

    //\\//\\//\\//\\//\\//\\//\\
	//    4.RECONSTRUCTION    \\
	//\\//\\//\\//\\//\\//\\//\\

    auto recon_start = high_resolution_clock::now();

    vector<future<int>> reconFutures;
    for (int j = 0; j < t; ++j) {
        reconFutures.push_back(async(launch::async, &Participant::startSecretReconstruction, participants[j]));
    }

    for (int j = 0; j < t; ++j) {
        int result = reconFutures[j].get();
    }

    auto recon_stop = high_resolution_clock::now();

    //\\//\\//\\//\\//\\//\\//\\
	//       5. REFRESH       \\
	//\\//\\//\\//\\//\\//\\//\\

    /*
    for (auto p : participants) {
        cout << p->id << "'s share is: " << p->getSecret() << endl;
    }
    */

    auto refresh_start = high_resolution_clock::now();

    vector<thread> refreshThreads;
    for (auto p : participants) {
        refreshThreads.push_back(thread(&Participant::startproactiveRefresh, p));
    }

    for (auto& x : refreshThreads) {
        x.join();
    }

    auto refresh_stop = high_resolution_clock::now();

    for (auto p : participants) p->stopReceiving = true;
    for (auto& x : threads) x.join();

    /*
    for (auto p : participants) {
        cout << p->id << "'s share is: " << p->getSecret() << endl;
    }
    */
    //\\//\\//\\//\\//\\//\\//\\
	//       6. RESULTS       \\
	//\\//\\//\\//\\//\\//\\//\\

    auto setup_dur = duration_cast<milliseconds>(setup_stop - setup_start).count();
    auto sec_gen_dur = duration_cast<milliseconds>(sec_gen_stop - sec_gen_start).count();
    double avg_share_gen = (NUM_OF_PARTICIPANTS - t) > 0 ? (double)total_share_gen_time / (NUM_OF_PARTICIPANTS - t) : 0;
    auto recon_dur = duration_cast<milliseconds>(recon_stop - recon_start).count();
    auto refresh_dur = duration_cast<milliseconds>(refresh_stop - refresh_start).count();

    cout << setup_dur << "," << sec_gen_dur << "," << avg_share_gen << "," << total_share_gen_time << "," << recon_dur << "," << refresh_dur << endl;

    /*
    cout << "1. Setup: " << setup_dur << " ms" << endl;
    cout << "2. Key Generation: " << sec_gen_dur << " ms" << endl;
    cout << "3. Share Generation (average): " << avg_share_gen << " ms" << endl;
    cout << "3. Share Generation (total): " << total_share_gen_time << " ms" << endl;
    cout << "4. Secret Reconstruction: " << recon_dur << " ms" << endl;
    cout << "5. Proactive Refresh: " << refresh_dur << " ms" << endl;
    */

    for (auto p : participants) delete p;

    return 0;
}
