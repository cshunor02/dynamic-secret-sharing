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
		int id;
		int secret; // random field element
		PublicKey pk;
		SecretKey sk;
		Polynomial polynomial;
	public:
		int port;
		Participant(int s, int mod, int id) : polynomial(mod) {
			secret = s;
			this->id = id;
		}

		void startPolynomialCalculation(int connected) {
			polynomial.generateRandomPolynomial(connected - 1);
			return;
		}

		// Get the which's user point
		int getPointValue(int which) {
			return polynomial.getPolynomial(secret, which - 1);
		}

		void getKeyPairs(Bulletin &board) {
			board.E.generateKeys(id, sk, pk);
			
			board.public_keys.push_back(pk);

			//board.printKey(pk);
		}

		SecretKey getSk() {
			return sk;
		}
};