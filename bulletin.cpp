#include "seal/seal.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <sstream>

using namespace std;
using namespace seal;

#include "homomorphic.cpp"

class Bulletin {
public:
	Homomorphic E;
	vector<PublicKey> public_keys;
	vector<vector<Ciphertext>> shares;
	int modulo;

	Bulletin() {
	}

	// t participants agree on an additive homomorphic encryption E and add E to the public bulletin board
	void initialize(size_t security, int modulo) {
		E.setParameters(security, modulo);
		this->modulo = modulo;
		return;
	}

	
	
};