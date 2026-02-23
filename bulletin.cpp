#include "seal/seal.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <sstream>

#include <zmq.hpp>
#include <zmq_addon.hpp>

using namespace std;
using namespace seal;
using namespace zmq;

#include "homomorphic.cpp"

class Bulletin {
public:
	Homomorphic E;
	vector<PublicKey> public_keys;
	vector<vector<Ciphertext>> shares;
	vector<int> ids;
	vector<array<int, 2>> points;

	int modulo;
	int t;

	Bulletin() {
	}

	// t participants agree on an additive homomorphic encryption E and add E to the public bulletin board
	void initialize(size_t security, int modulo) {
		E.setParameters(security, modulo);
		this->modulo = modulo;
		return;
	}

	// TODO : sum up the shares, after everyone uploaded their shares
	/*
	vector<Ciphertext> totals;
    for (int i = 0; i < t; ++i) {
        totals.push_back(bullet.E.sumShares({ bullet.shares[0][i], bullet.shares[1][i], bullet.shares[2][i] }));
    }
	*/
};