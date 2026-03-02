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
		int secret;							// random field element
		PublicKey pk;						// Public key
		SecretKey sk;						// Secret key
		Bulletin &bullet;					// the pointer of the board where the shares can be uploaded
		int summedPiece;					// the sum of the shared secrets
		context_t context;					
		socket_t broker;
		pollitem_t items[1];
		vector<int> maskBuffer, sumMaskBuffer;
		vector<Ciphertext> leaderBuffer;
		mutex maskMutex, leaderMutex;
		condition_variable maskCv, leaderCv;

	public:
		bool leader = false;				// is the Participant a leader?
		bool stopReceiving = false;
		int id;
		Polynomial polynomial;
		string location;
		Participant(int s, int mod, int given_id, Bulletin &board) : context(1), broker(context, ZMQ_ROUTER), polynomial(mod), bullet(board) {
			id = given_id;
			secret = s;
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

		Ciphertext newUserJoin(int newUserId, PublicKey newUserPK, int share, vector<array<int, 2>> points, int modulo, Homomorphic& E, int mask) {
			double lambda = Polynomial::lagrangeBasisPolynomial(points, id, newUserId);

			int newShare = ((int)round(share * lambda) + mask ) % modulo;
			while (newShare < 0) {
				newShare += modulo;
			}
			return E.encryptSecret(newUserPK, newShare);
		}

		// https://zguide.zeromq.org/docs/chapter3/#The-DEALER-to-REP-Combination
		void startServer() {
			broker.bind(location);
			this->location = broker.get(sockopt::last_endpoint);

			broker.set(sockopt::rcvtimeo, 200); // Time limit

			try {
				while (!stopReceiving) {
					poll(items, 1, chrono::milliseconds(10));

					if (items[0].revents & ZMQ_POLLIN) {
						receiveCipher();
					}
				}
			}
			catch (exception &e) {}
		}

		int getSecret() {
			return secret;
		}

		void sendMsg(string dest, const Ciphertext& piece, string type) {
			socket_t worker(context, ZMQ_DEALER);
			worker.connect(dest);

			stringstream ss;
			piece.save(ss);
			string ssd = ss.str();

			worker.send(buffer(type), send_flags::sndmore);
			worker.send(buffer(to_string(this->id)), send_flags::sndmore);
			worker.send(buffer(ssd), send_flags::none);
			return;
		}

		void sendMsg(string dest, int piece, string type) {
			socket_t worker(context, ZMQ_DEALER);
			worker.connect(dest);

			worker.send(buffer(type), send_flags::sndmore);
			worker.send(buffer(to_string(this->id)), send_flags::sndmore);
			worker.send(buffer(to_string(piece)), send_flags::none);
			return;
		}

		//https://stackoverflow.com/questions/73803967/zmqmessage-t-assign-a-string
		void receiveCipher() {
			// Receive the id, an empty frame, and the serialized Ciphertext
			message_t identity;
			message_t empty;
			message_t type, id, msg;

			broker.recv(identity, recv_flags::none);
			broker.recv(type, recv_flags::none);
			broker.recv(id, recv_flags::none);
			broker.recv(msg, recv_flags::none);

			string typeSwitch = type.to_string();
			string senderId = id.to_string();
			string data(static_cast<char*>(msg.data()), msg.size()); //https://www.geeksforgeeks.org/cpp/static_cast-in-cpp/

			if (typeSwitch == "enc") {
				stringstream ss(data);
				Ciphertext result;
				bullet.E.loadCiphertext(result, ss);
				lock_guard<mutex> lock(leaderMutex);
				leaderBuffer.push_back(result);
				leaderCv.notify_one();
			}
			else if (typeSwitch == "msk") {
				lock_guard<mutex> lock(maskMutex);
				maskBuffer.push_back(stoi(data));
				maskCv.notify_one();
			}
			else if (typeSwitch == "sum") {
				lock_guard<mutex> lock(leaderMutex);
				sumMaskBuffer.push_back(stoi(data));
				leaderCv.notify_one();
			}
			else if (typeSwitch == "new") {
				stringstream ss(data);
				Ciphertext newShare;
				bullet.E.loadCiphertext(newShare, ss);

				decryptAndSaveSecret(newShare);
			}
			else {
				cout << "typeSwitch: " << typeSwitch << endl;
				throw new exception;
			}
			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//  2. SECRET GENERATION  \\
		//\\//\\//\\//\\//\\//\\//\\

		void startSecretGeneration() {
			getKeyPairs(bullet);	// For each i, Pi generates a pair of key pki, ski

			bullet.shares.push_back({ id, bullet.E.encryptSecret(pk, secret) });
			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//   3.SHARE GENERATION   \\
		//\\//\\//\\//\\//\\//\\//\\

		void startShareGeneration() {
			// Share generation Step 2-4

			int mask = rand() % bullet.modulo;						// Step 4

			int newUserId = bullet.ids.back();						// Step 2
			PublicKey newUserPk = *(bullet.public_keys.end() - 1);  // Step 2
			int max_users = bullet.ids.size() - 1;


			Ciphertext newPiece = newUserJoin(						// Step 3 and 5
				newUserId,
				newUserPk,
				secret,
				bullet.points,
				bullet.modulo,
				bullet.E,
				mask
			);

			// TODO : Send newPiece to Leader
			if (leader) {
				// receive pieces -> HOW?
				unique_lock<mutex> lock(leaderMutex);
				leaderBuffer.push_back(newPiece);
			}
			else {
				// ZMQ the pieces
				sendMsg(bullet.destinations[bullet.leaderId], newPiece, "enc");
			}

			// Additive MPC for mask - Step 7 part 1

			vector<int> maskPieces(max_users);
			int sum = 0;
			for (int i = 0; i < max_users - 1; ++i) {
				maskPieces[i] = rand() % bullet.modulo;
				sum = (sum + maskPieces[i]) % bullet.modulo;
			}

			maskPieces[max_users - 1] = (mask - sum + bullet.modulo) % bullet.modulo;

			// Send out the pieces to the users

			for (int i = 0; i < max_users; ++i) {
				if (bullet.ids[i] == id) {
					lock_guard<mutex> lock(maskMutex);
					maskBuffer.push_back(maskPieces[i]);
				}
				else {
					sendMsg(bullet.destinations[i], maskPieces[i], "msk");
				}
			}

			unique_lock<mutex> lock(maskMutex);
			maskCv.wait(lock, [this, max_users] {
				return maskBuffer.size() >= max_users;
			});

			sum = 0;
			for (int x : maskBuffer) {
				sum += x;
				sum %= bullet.modulo;
			}

			// Send mask sum to Leader - Step 7 part 2

			if (leader) {
				{
					lock_guard<mutex> lock(leaderMutex);
					sumMaskBuffer.push_back(sum);
				}
				leaderTasks();
			}
			else {
				sendMsg(bullet.destinations[bullet.leaderId], sum, "sum"); // Step 8
			}

			return;
		}

		void leaderTasks() {
			int max_users = bullet.ids.size() - 1;

			unique_lock<mutex> lock(leaderMutex);
			leaderCv.wait(lock, [this, max_users] {
				return leaderBuffer.size() >= max_users &&
					sumMaskBuffer.size() >= max_users;
			});

			int sum = 0;

			// Step 9
			for (int x : sumMaskBuffer) {
				sum += x;
				sum %= bullet.modulo;
			}

			// Step 10
			sum *= -1;
			while (sum < 0) {
				sum += bullet.modulo;
			}

			Ciphertext encMask = bullet.E.encryptSecret(*(bullet.public_keys.end() - 1), sum);

			Ciphertext sumEncShares = bullet.E.sumShares(leaderBuffer);

			Ciphertext newUserShare;
			bullet.E.addShares(encMask, sumEncShares, newUserShare);

			sendMsg(bullet.destinations.back(), newUserShare, "new");

			return;
		}

		void decryptAndSaveSecret(Ciphertext newShare) {
			int new_piece;
			bullet.E.decryptSecret(sk, newShare, new_piece);
			secret = new_piece;
			return;
		}
};