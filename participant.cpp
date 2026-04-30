#include "seal/seal.h"
#include <vector>
#include <array>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include <thread>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>

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
		PublicKey pk;						// Public key
		SecretKey sk;						// Secret key
		Bulletin &bullet;					// the pointer of the board where the shares can be uploaded
		int summedPiece;					// the sum of the shared secrets

		// ZeroMQ
		context_t context;					// to manage threads in ZeroMQ 
		socket_t broker;					// ROUTER socket for communication
		pollitem_t items[1];				// to check incoming messages on the socket

		vector<int> maskBuffer, sumMaskBuffer, reconstructBuffer; // buffers for messages
		vector<Ciphertext> leaderBuffer;

		mutex maskMutex, leaderMutex, reconstructMutex; // synchronization primitives
		condition_variable maskCv, leaderCv, reconstructCv; // synchronization primitives

	public:
		atomic<size_t> total_bytes_received{ 0 };
		bool leader = false;				// determine if the Participant is a leader
		bool stopReceiving = false;			// stop receiving messages
		int id;								// the Participant's unique identification number
		string location;					// the location of the user (localhost:port)
		Participant(int mod, int given_id, Bulletin &board) : context(1), broker(context, ZMQ_ROUTER), bullet(board) {
			id = given_id;
			items[0] = { static_cast<void*>(broker), 0, ZMQ_POLLIN, 0 };
		}

		// In Setup, the Participant generates their pk and sk keypairs
		void getKeyPairs(Bulletin &board) {
			board.E.generateKeys(id, sk, pk);
			board.public_keys.push_back(pk);
		}

		// When a new participant joins to the calculation
		// the function generates their share
		Ciphertext newUserJoin(int newUserId, PublicKey newUserPK, int share, vector<array<int, 2>> points, int modulo, Homomorphic& E, int mask) {
			double lambda = Polynomial::lagrangeBasisPolynomial(points, id, newUserId);

			int newShare = ((int)round(share * lambda) + mask ) % modulo;
			while (newShare < 0) {
				newShare += modulo;
			}
			return E.encryptSecret(newUserPK, newShare);
		}

		// Starting the communication node
		// https://zguide.zeromq.org/docs/chapter3/#The-DEALER-to-ROUTER-Combination
		void startServer() {
			broker.bind(location);
			this->location = broker.get(sockopt::last_endpoint);

			broker.set(sockopt::rcvtimeo, 100); // Time limit to check if socket starts

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

		// To send the Ciphertext as serialized message
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

		// To send an integer to another participant
		void sendMsg(string dest, int piece, string type) {
			socket_t worker(context, ZMQ_DEALER);
			worker.connect(dest);

			worker.send(buffer(type), send_flags::sndmore);
			worker.send(buffer(to_string(this->id)), send_flags::sndmore);
			worker.send(buffer(to_string(piece)), send_flags::none);
			return;
		}

		// To handle message receiving
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

			total_bytes_received += (identity.size() + type.size() + id.size() + msg.size());

			string typeSwitch = type.to_string();
			string senderId = id.to_string();
			string data(static_cast<char*>(msg.data()), msg.size()); //https://www.geeksforgeeks.org/cpp/static_cast-in-cpp/
			
			if (typeSwitch == "enc") { // sharing a piece of the new share in Share Generation
				stringstream ss(data);
				Ciphertext result;
				bullet.E.loadCiphertext(result, ss);
				lock_guard<mutex> lock(leaderMutex);
				leaderBuffer.push_back(result);
				leaderCv.notify_one();
			}
			else if (typeSwitch == "msk") {  // Adding up the shares, to get the sum of the masks
				lock_guard<mutex> lock(maskMutex);
				maskBuffer.push_back(stoi(data));
				maskCv.notify_one();
			}
			else if (typeSwitch == "sum") {  // Sending out the summed mask to the Leader
				lock_guard<mutex> lock(leaderMutex);
				sumMaskBuffer.push_back(stoi(data));
				leaderCv.notify_one();
			}
			else if (typeSwitch == "new") {  // The new user receives their secret share
				stringstream ss(data);
				Ciphertext newShare;
				bullet.E.loadCiphertext(newShare, ss);

				decryptAndSaveSecret(newShare);
			}
			else if (typeSwitch == "rec") { // For recovering the secret
				lock_guard<mutex> lock(reconstructMutex);
				reconstructBuffer.push_back(stoi(data));
				reconstructCv.notify_one();
			}
			else {
				return;
			}
			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//  2. SECRET GENERATION  \\
		//\\//\\//\\//\\//\\//\\//\\

		// Uploading the public key to the bulletin board
		void startSecretGeneration(int secret) {
			bullet.shares.push_back(bullet.E.encryptSecret(pk, secret));
			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//   3.SHARE GENERATION   \\
		//\\//\\//\\//\\//\\//\\//\\

		void startShareGeneration() {
			int max_users = bullet.t;

			int temp = -1;
			for (int i = 0; i < max_users; ++i) {
				if (bullet.ids[i] == id) {
					temp = i;
					break;
				}
			}
			if (temp == -1) {
				return;
			}
			// Share generation Step 2-4

			int mask = rand() % bullet.modulo;						// Step 4
			int newUserId = bullet.ids.back();						// Step 2
			PublicKey newUserPk = *(bullet.public_keys.end() - 1);  // Step 2

			vector<array<int, 2>> tNumberOfPoints;
			for (int i = 0; i < max_users; ++i) {
				tNumberOfPoints.push_back(bullet.points[i]);
			}

			int secret = downloadFromBoard(this->id);

			Ciphertext newPiece = newUserJoin(						// Step 3 and 5
				newUserId,
				newUserPk,
				secret,
				tNumberOfPoints,
				bullet.modulo,
				bullet.E,
				mask
			);

			if (leader) {
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

			maskBuffer.clear();

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

		// If the user is the leader, run the function
		void leaderTasks() {
			int max_users = bullet.t;

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

			sumMaskBuffer.clear();
			leaderBuffer.clear();

			Ciphertext newUserShare;
			bullet.E.addShares(encMask, sumEncShares, newUserShare);

			bullet.shares.push_back(newUserShare);
			sendMsg(bullet.destinations.back(), newUserShare, "new");

			return;
		}

		// After getting the message as the new user
		// the new user can decrypt their share
		void decryptAndSaveSecret(Ciphertext newShare) {
			int new_piece;
			bullet.E.decryptSecret(sk, newShare, new_piece);
			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//    4.RECONSTRUCTION    \\
		//\\//\\//\\//\\//\\//\\//\\

		int startSecretReconstruction() {
			int max_users = bullet.t;
			int secret = downloadFromBoard(this->id);

			int temp = -1;
			for (int i = 0; i < max_users; ++i) {
				if (bullet.ids[i] == id) {
					temp = i;
					break;
				}
			}
			if (temp == -1) {
				return 0;
			}

			vector<std::array<int, 2>> points(bullet.points.begin(), bullet.points.begin() + bullet.t);

			int lambda = Polynomial::lagrangeBasisPolynomial(points, id, 0);
			lambda %= bullet.modulo;

			temp = (secret * lambda) % bullet.modulo;

			for (int i = 0; i < max_users; ++i) {
				if (bullet.ids[i] == id) {
					lock_guard<mutex> lock(reconstructMutex);
					reconstructBuffer.push_back(temp);
				}
				else {
					sendMsg(bullet.destinations[i], temp, "rec");
				}
			}

			unique_lock<mutex> lock(reconstructMutex);
			reconstructCv.wait(lock, [this, max_users] {
				return reconstructBuffer.size() >= max_users;
			});

			temp = 0;
			for (int x : reconstructBuffer) {
				temp += x;
				temp %= bullet.modulo;
			}
			
			reconstructBuffer.clear();

			return temp;
		}

		// To decrypt the downloaded share from the bulletin board
		int downloadFromBoard(int id) {
			int temp;
			bullet.E.decryptSecret(sk, bullet.shares[id-1], temp);
			return temp;
		}
};