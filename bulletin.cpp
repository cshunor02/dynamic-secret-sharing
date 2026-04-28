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
	Homomorphic E;	// Homomorphic encryption
	vector<PublicKey> public_keys; 	// The vector of the Participant's public keys
	vector<Ciphertext> shares;		// The secret shares of the Participants
	vector<int> ids;				// The IDs of the Participants
	vector<array<int, 2>> points;	// The ids and the shares are combined
	vector<string> destinations;	// The port numbers of the Participants
	int leaderId;					// The ID number of the leader Participant
	int modulo;						// The modulo M number
	int t;							// The threshold t number

	// Default constructor
	Bulletin() {}

	// t participants agree on an additive homomorphic encryption E
	void initialize(size_t security, int modulo, int treshold) {
		E.setParameters(security, modulo);
		this->modulo = modulo;
		this->t = treshold;
		return;
	}
};