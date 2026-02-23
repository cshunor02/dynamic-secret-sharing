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
	public:
		bool stopReceiving = false;
		int id;
		Polynomial polynomial;
		string location;
		Participant(int s, int mod, int given_id, Bulletin &board) : polynomial(mod), bullet(board) {
			secret = s;
			id = given_id;
		}

		void startPolynomialCalculation(int connected) {
			polynomial.generateRandomPolynomial(connected - 1);
			return;
		}

		// Get the which's user point
		int getPointValue(int which) {
			return polynomial.getPolynomial(secret, which);
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
			context_t context(1);
			socket_t broker(context, ZMQ_ROUTER);

			broker.bind("tcp://127.0.0.1:*");

			cout << "Server has been started for " << this->id << endl;

			broker.set(sockopt::rcvtimeo, 200); // Time limit

			string getLocation = broker.get(sockopt::last_endpoint);
			this->location = getLocation;

			pollitem_t items[] = {
				{ broker, 0, ZMQ_POLLIN, 0 } };

			startSecretGeneration();
			startShareGeneration();

			/*
			try {
				while (!stopReceiving) {
					poll(items, 1, chrono::milliseconds(10));

					if (items[0].revents & ZMQ_POLLIN) {
						receiveMessage(broker);
					}
				}
			}
			catch (exception &e) {}
			*/
		}

		//https://stackoverflow.com/questions/73803967/zmqmessage-t-assign-a-string
		void receiveMessage(socket_t& broker) {
			message_t identity;
			message_t msg;
			broker.recv(identity, recv_flags::none);
			broker.recv(msg, recv_flags::none);

			string data(static_cast<char*>(msg.data()), msg.size()); //https://www.geeksforgeeks.org/cpp/static_cast-in-cpp/
			stringstream ss(data);

			Ciphertext result;
			bullet.E.loadCiphertext(result, ss);

			int temp;
			bullet.E.decryptSecret(getSk(), result, temp);

			cout << temp << endl;
			return;
		}

		void sendMessage(string dest, const Ciphertext& piece) {
			context_t context(1);
			socket_t worker(context, ZMQ_DEALER);

			worker.connect(dest);

			stringstream ss;
			piece.save(ss);
			string ssd = ss.str();

			cout << "Sending secret piece (" << ssd.size() << " bytes)..." << endl;
			worker.send(buffer(ssd));
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
				int temp = getPointValue(bullet.ids[i]);
				collect.push_back(bullet.E.encryptSecret(bullet.public_keys[i], temp));
			}
			bullet.shares.push_back(collect);

			// Download shares

			// TODO : wait until bullet.shares reaces max_users size
			//        after it, download TOTAL

			Ciphertext total;

			bullet.E.decryptSecret(getSk(), total, summed_piece);

			// TODO : store result in private
			return;
		}

		//\\//\\//\\//\\//\\//\\//\\
		//   3.SHARE GENERATION   \\
		//\\//\\//\\//\\//\\//\\//\\

		void startShareGeneration() {
			int mask = rand();

			Ciphertext newPiece = newUserJoin(
				bullet.ids.back(),
				*(bullet.public_keys.end() - 1),
				summed_piece,
				bullet.points,
				bullet.modulo,
				bullet.E,
				mask
			);

			// TODO : Multiparti computation for SUM masks

			int sum_masks = 0;

			sum_masks *= -1;
			sum_masks %= bullet.modulo;

			while (sum_masks < 0) {
				sum_masks += bullet.modulo;
			}

			// TODO : Only leader does the following:

			// new_shares is what the other clients calculated in newPiece and uploaded in the bullet

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

			/*
			* New user can decrypt it: 
			* 
			int new_piece;
			bullet.E.decryptSecret(Dave.getSk(), dave_share, new_piece);
			*/
		}
};