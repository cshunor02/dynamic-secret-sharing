#include "seal/seal.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;
using namespace seal;

class Participant {
	private:
		int id;
		int secret; // random field element
		PublicKey pk;
		SecretKey sk;
		vector<int> polynomial;
	public:
		int port;
		Participant(int s) {
			secret = s;
		}

		void generateRandomPolynomial(int initial_users) {
			srand(time(0));
			for (int i = 0; i < initial_users; i++) {
				polynomial.push_back(rand() % int(rand() * 100000));
			}
			return;
		}

		void newUserPolynomialPoint() {
			srand(time(0));
			polynomial.push_back(rand() % int(rand() * 100000));
			return;
		}

		vector<int> getPolynomial() {
			return polynomial;
		}
};