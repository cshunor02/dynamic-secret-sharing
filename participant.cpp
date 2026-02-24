#include "seal/seal.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <string>

#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <sstream>

#include "polynomial.cpp"
#include "bulletin.cpp"

using namespace std;
using namespace seal;
using namespace zmq;

class Participant {
	private:
		int secret; // random field element
		PublicKey pk;
		SecretKey sk;
		bool leader = false;
		Bulletin &bullet;
		int summed_piece;
		context_t context;
		socket_t broker;
		pollitem_t items[1];
		vector<array<int, 2>> maskBuffer;  // For sharing the mask
		mutex maskMutex;			       // For sharing the mask
		condition_variable maskCv;		   // For sharing the mask
	public:
		bool stopReceiving = false;
		int id;
		Polynomial polynomial;
		string location;
		Participant(int s, int mod, int given_id, Bulletin &board) : context(1), broker(context, ZMQ_ROUTER), polynomial(mod), bullet(board) {
			id = given_id;
			items[0] = { static_cast<void*>(broker), 0, ZMQ_POLLIN, 0 };
		}

		void startPolynomialCalculation(int connected) {
			polynomial.generateRandomPolynomial(connected - 1);
			return;
		}

		// Get the which's user point
		int getPointValue(int piece, int which) {
			return polynomial.getPolynomial(piece, which);
		}

		void getKeyPairs(Bulletin &board) {
			board.E.generateKeys(id, sk, pk);
			board.public_keys.push_back(pk);
		}

		SecretKey getSk() {
			return sk;
		}

		Ciphertext newUserJoin(int newUserId, PublicKey newUserPK, int share, vector<array<int, 2>> points, int modulo, Homomorphic& E, int mask) {
			double lambda = Polynomial::lagrangeBasisPolynomial(points, id, newUserId);

			int newShare = ((int)round(share * lambda) + mask )% modulo;
			while (newShare < 0) {
				newShare += modulo;
			}
			return E.encryptSecret(newUserPK, newShare);
		}

		// https://zguide.zeromq.org/docs/chapter3/#The-DEALER-to-REP-Combination
		void startServer() {
			broker.bind("tcp://127.0.0.1:*");
			this->location = broker.get(sockopt::last_endpoint);

			broker.set(sockopt::rcvtimeo, 200); // Time limit

			startSecretGeneration();
			startShareGeneration();

			/*
			try {
				while (!stopReceiving) {
					poll(items, 1, chrono::milliseconds(10));

					if (items[0].revents & ZMQ_POLLIN) {
						receiveCipher(broker);
					}
				}
			}
			catch (exception &e) {}
			*/
		}

		void sendCipher(string dest, const Ciphertext& piece) {
			socket_t worker(context, ZMQ_DEALER);
			worker.connect(dest);

			stringstream ss;
			piece.save(ss);
			string ssd = ss.str();

			worker.send(buffer(ssd)	, send_flags::none);

			message_t ack;
			worker.set(sockopt::rcvtimeo, 10);
			worker.recv(ack, recv_flags::none);
		}

		//https://stackoverflow.com/questions/73803967/zmqmessage-t-assign-a-string
		void receiveCipher(socket_t& broker) {
			message_t identity;
			message_t msg;
			broker.recv(identity, recv_flags::none);
			broker.recv(msg, recv_flags::none);

			string data(static_cast<char*>(msg.data()), msg.size()); //https://www.geeksforgeeks.org/cpp/static_cast-in-cpp/
			stringstream ss(data);
			Ciphertext result;
			bullet.E.loadCiphertext(result, ss);

			int temp;
			bullet.E.decryptSecret(sk, result, temp);

			{
				lock_guard<mutex> lock(maskMutex);
				maskBuffer.push_back({ stoi(identity.to_string()) , temp });
			}
			maskCv.notify_one();

			broker.send(identity, send_flags::sndmore);
			broker.send(str_buffer("ACK"), send_flags::none);

			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//  2. SECRET GENERATION  \\
		//\\//\\//\\//\\//\\//\\//\\

		void startSecretGeneration() {
			getKeyPairs(bullet); // For each i, Pi generates a pair of key pki, ski
			startPolynomialCalculation(bullet.t); // Participants generate p(x) polynomials at degree t

			int max_users = bullet.ids.size();

			vector<Ciphertext> collect;
			for (int i = 0; i < max_users; ++i) {
				collect.push_back(bullet.E.encryptSecret(bullet.public_keys[i], getPointValue(secret, bullet.ids[i])));
			}
			bullet.shares.push_back(collect);

			try {
				while (true) {
					poll(items, 1, chrono::milliseconds(10));

					if (items[0].revents & ZMQ_POLLIN) {
						receiveCipher(broker);
					}
				}
			}
			catch (exception& e) {}
			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//   3.SHARE GENERATION   \\
		//\\//\\//\\//\\//\\//\\//\\

		void startShareGeneration() {
			int mask = rand() % bullet.modulo;

			Ciphertext newPiece = newUserJoin(
				bullet.ids.back(),
				*(bullet.public_keys.end() - 1),
				summed_piece,
				bullet.points,
				bullet.modulo,
				bullet.E,
				mask
			);

			// TODO : Upload newPiece to Bulletin

			int max_users = bullet.ids.size();

			// MPC for mask
			polynomial.generateRandomPolynomial(max_users - 1);

			for (int i = 0; i < max_users; ++i) {
				if (bullet.ids[i] == id) continue;
				sendCipher(bullet.destinations[i], bullet.E.encryptSecret(bullet.public_keys[i], getPointValue(mask, bullet.ids[i])));
			}
			
			unique_lock<mutex> lock(maskMutex);
			maskCv.wait(lock, [this] {
				return maskBuffer.size() >= (bullet.ids.size() - 1);
			});

			int sum_masks = Polynomial::lagrange(maskBuffer, bullet.modulo);

			sum_masks *= -1;
			sum_masks %= bullet.modulo;

			while (sum_masks < 0) {
				sum_masks += bullet.modulo;
			}

			if (leader) leaderTasks(sum_masks);
			return;
		}

		void leaderTasks(int sum_masks) {
			// new_shares is what the other clients calculated in newPiece() and uploaded to the bullet

			vector<Ciphertext> new_shares;
			int max_users = bullet.ids.size();

			for (int i = 0; i < max_users; ++i) {
				new_shares.push_back(bullet.shares[i][max_users]);
			}

			Ciphertext result = bullet.E.sumShares(new_shares);
			Ciphertext mask_enc = bullet.E.encryptSecret(*(bullet.public_keys.end() - 1), sum_masks);

			Ciphertext newUserShare;
			bullet.E.addShares(result, mask_enc, newUserShare);

			// TODO : Leader sends this to the new user

			sendCipher(bullet.destinations[max_users - 1], newUserShare);

			return;
		}

		void decryptAndSaveSecret(Ciphertext newShare) {
			int new_piece;
			bullet.E.decryptSecret(getSk(), newShare, new_piece);
			secret = new_piece;
			return;
		}
};