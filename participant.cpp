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

#include "polynomial.cpp"
#include "bulletin.cpp"

using namespace std;
using namespace seal;

class Participant {
	private:
		int secret; // random field element
		PublicKey pk;
		SecretKey sk;
	public:
		int id;
		Polynomial polynomial;
		int port;
		Participant(int s, int mod, int given_id) : polynomial(mod) {
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

		Ciphertext newUserJoin(int newUserId, PublicKey newUserPK, int share, vector<array<int, 2>> points, Bulletin& board) {
			double lambda = Polynomial::lagrangeBasisPolynomial(points, id, newUserId);

			int newShare = (int)round(share * lambda) % board.modulo;
			if (newShare < 0) {
				newShare += board.modulo;
			}
			return board.E.encryptSecret(newUserPK, newShare);
		}
};