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

	Bulletin() {
	}

	void initialize(size_t security) {
		E.setParameters(security);
		return;
	}

	void printKey(PublicKey key) {
		stringstream ss;
		key.save(ss);
		cout << "Public Key Size: " << ss.str().size() << " bytes" << endl;
		return;
	}
	
};