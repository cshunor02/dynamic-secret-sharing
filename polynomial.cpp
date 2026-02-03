#include "seal/seal.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>

using namespace std;
using namespace seal;

#define MAX_RAND 40

class Polynomial {
private:
	vector<int> polynomial;
	int modulo;
public:
	int port;
	Polynomial() {

	}
	Polynomial(int mod) {
		modulo = mod;
	}

	void generateRandomPolynomial(int initial_users) {
		srand(time(0));
		for (int i = 0; i < initial_users; i++) {
			polynomial.push_back((rand() % int(rand() * MAX_RAND)) % modulo);
		}
		return;
	}

	void newUserPolynomialPoint() {
		srand(time(0));
		polynomial.push_back((rand() % int(rand() * MAX_RAND)) % modulo);
		return;
	}

    // Get which's users' polynomial data
	int getPolynomial(int secret, int which) {
		int temp = secret;
		for (int j = 0; j < polynomial.size(); j++) {
			temp += polynomial[j] * pow(which, j + 1);
		}
		return temp % modulo;
	}
};