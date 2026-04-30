#include <iostream>
#include <ctime>
#include <vector>
#include <stdexcept>
#include <future>
#include <string>
#include <thread>
#include <chrono>

#include "participant.cpp"

using namespace std;
using namespace zmq;
using namespace chrono;

int main(int argc, char* argv[]) {
    //\\//\\//\\//\\
    //  1. SETUP  \\
    //\\//\\//\\//\\

    int NUM_OF_PARTICIPANTS = 3;
    int t = 2;
    int MODULO = 1021;

    try{
        if (argc >= 4) {
            NUM_OF_PARTICIPANTS = stoi(argv[1]);
            t = stoi(argv[2]);
            MODULO = stoi(argv[3]);
        }    
    } catch (const invalid_argument& e) {
        return 1;
    } catch(const out_of_range& e) {
        return 1;
    }
    
    srand(time(0));
    Bulletin bullet;

    auto setup_start = high_resolution_clock::now(); // To measure Setup: start
    bullet.initialize(4096, MODULO, t);              // the secrecy of E, modulo

    vector<Participant*> participants;

    for (int i = 0; i < t; ++i) {
        int id = i + 1;

        Participant* p = new Participant(MODULO, id, bullet);
        p->location = "tcp://127.0.0.1:" + to_string(5550 + id); // The ports are starting from 5550
        participants.push_back(p);

        bullet.ids.push_back(id);
        bullet.destinations.push_back(p->location);
        bullet.points.push_back({ id, 0 });
        p->getKeyPairs(bullet);
    }
    auto setup_stop = high_resolution_clock::now(); // To measure Setup: stop

    //\\//\\//\\//\\//\\//\\//\\
    //  2. SECRET GENERATION  \\
    //\\//\\//\\//\\//\\//\\//\\
    
    auto sec_gen_start = high_resolution_clock::now();  // To measure Secret Generation: start
    for (auto p : participants) {
        int secret = rand() % MODULO;
        p->startSecretGeneration(secret);
    }
    auto sec_gen_stop = high_resolution_clock::now();  // To measure Secret Generation: stop

    //\\//\\//\\//\\//\\//\\//\\
    //   3.SHARE GENERATION   \\
    //\\//\\//\\//\\//\\//\\//\\

    // The leader is randomly selected
    bullet.leaderId = rand() % t;
    participants[bullet.leaderId]->leader = true; // We set True to the participant who the leader is
    vector<thread> threads;

    for (auto p : participants) {
        threads.push_back(thread(&Participant::startServer, p));
    }

    this_thread::sleep_for(milliseconds(500)); // A small waiting to make sure that the threads start

    long long total_share_gen_time = 0;

    for (int i = 0; i < (NUM_OF_PARTICIPANTS - t); ++i) {
        vector<thread> workerThreads;
        Participant* p = new Participant(MODULO, t + 1 + i, bullet);
        p->location = "tcp://127.0.0.1:" + to_string(5550 + t + 1 + i);

        p->getKeyPairs(bullet);
        participants.push_back(p);
        bullet.ids.push_back(p->id);
        bullet.destinations.push_back(p->location);
        bullet.points.push_back({ t + 1 + i, 0 });

        threads.push_back(thread(&Participant::startServer, p));
        this_thread::sleep_for(milliseconds(100));

        auto single_share_start = high_resolution_clock::now(); // To measure Share Generation: start
        
        for (int j = 0; j < t; ++j) {
            workerThreads.push_back(thread(&Participant::startShareGeneration, participants[j]));
        }

        for (auto& x : workerThreads) {
            x.join();
        }

        auto single_share_stop = high_resolution_clock::now();  // To measure Share Generation: stop
        total_share_gen_time += duration_cast<milliseconds>(single_share_stop - single_share_start).count(); // Adding up the share generations for all the participants
    }

    this_thread::sleep_for(seconds(1));

    //\\//\\//\\//\\//\\//\\//\\
	//    4.RECONSTRUCTION    \\
	//\\//\\//\\//\\//\\//\\//\\

    auto recon_start = high_resolution_clock::now(); // To measure Reconstruction: start

    vector<future<int>> reconFutures;
    for (int j = 0; j < t; ++j) {
        reconFutures.push_back(async(launch::async, &Participant::startSecretReconstruction, participants[j]));
    }

    for (int j = 0; j < t; ++j) {
        int result = reconFutures[j].get();
    }

    auto recon_stop = high_resolution_clock::now(); // To measure Reconstruction: stop

    //\\//\\//\\//\\//\\//\\//\\
	//       6. RESULTS       \\
	//\\//\\//\\//\\//\\//\\//\\

    for (auto p : participants) p->stopReceiving = true;
    for (auto& x : threads) x.join();

    auto setup_dur = duration_cast<milliseconds>(setup_stop - setup_start).count();
    auto sec_gen_dur = duration_cast<milliseconds>(sec_gen_stop - sec_gen_start).count();
    double avg_share_gen = (NUM_OF_PARTICIPANTS - t) > 0 ? (double)total_share_gen_time / (NUM_OF_PARTICIPANTS - t) : 0;
    auto recon_dur = duration_cast<milliseconds>(recon_stop - recon_start).count();

    double total_mb = 0;

    for (auto p : participants) {
        total_mb += p->total_bytes_received.load() / (1024.0 * 1024.0);
    }
    cout << setup_dur << "," << sec_gen_dur << "," << avg_share_gen << "," << total_share_gen_time << "," << recon_dur << "," << total_mb << endl;

    for (auto p : participants) delete p;

    return 0;
}
