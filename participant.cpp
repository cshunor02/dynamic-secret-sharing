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
	public:
		bool stopProcess = false;
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

			//board.printKey(pk);
		}

		SecretKey getSk() {
			return sk;
		}

		Ciphertext newUserJoin(int newUserId, PublicKey newUserPK, int share, vector<array<int, 2>> points, Bulletin& board, int mask) {
			double lambda = Polynomial::lagrangeBasisPolynomial(points, id, newUserId);

			int newShare = ((int)round(share * lambda) + mask )% board.modulo;
			if (newShare < 0) {
				newShare += board.modulo;
			}
			return board.E.encryptSecret(newUserPK, newShare);
		}

		void startServer() {
			context_t context{ 1 };

			socket_t socket{ context, zmq::socket_type::rep }; // rep is for reply
			socket.bind("tcp://127.0.0.1:*");

			cout << "Server has been started for " << this->id << endl;

			socket.set(sockopt::rcvtimeo, 200); // Time limit

			const string last_endpoint = socket.get(zmq::sockopt::last_endpoint);
			this->location = last_endpoint;

			for (;stopProcess == false;)
			{
				cout << "I've been waiting for a message " << this-> id << "..." << endl;
				message_t request;

				auto result = socket.recv(request, recv_flags::none);
				if (result) {
					cout << "Received secret piece!";

					string data(static_cast<char*>(request.data()), request.size());
					stringstream ss(data);

					Ciphertext result_cip;
					bullet.E.loadCiphertext(result_cip, ss);

					int temp;
					bullet.E.decryptSecret(getSk(), result_cip, temp);

					cout << " and the secret piece is: " << temp << endl;

					// send the reply to the client
					socket.send(str_buffer("ACK"));
					// simulate work
					this_thread::sleep_for(1s);
				}
			}
		}

		void sendMessage(string dest, const Ciphertext& piece) {
			context_t context{ 1 };
			socket_t socket(context, socket_type::req); // req is for request

			socket.set(sockopt::rcvtimeo, 2000);
			socket.set(sockopt::sndtimeo, 2000);

			socket.connect(dest);

			stringstream ss;
			piece.save(ss);
			string ssd = ss.str();

			cout << "Sending secret piece (" << ssd.size() << " bytes)..." << endl;
			socket.send(buffer(ssd), send_flags::none);

			// wait for reply from server
			message_t reply;
			auto result = socket.recv(reply, recv_flags::none);

			if (result) {
				cout << "[Client] Success! Server replied: " << reply.to_string() << endl;
			}
			else {
				cerr << "[Client] Error: Timeout or peer disconnected." << endl;
			}
		}
};