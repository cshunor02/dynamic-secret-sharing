#include "seal/seal.h"
#include <vector>
#include <cstdlib>
#include <cstring>

using namespace std;
using namespace seal;

#define MAX_RAND 40

class Polynomial {
private:
	int modulo = 7;
public:
	vector<int> polynomial;
	int port;
	Polynomial() : modulo(7) {

	}
	Polynomial(int mod) : modulo(mod) {}

	void generateRandomPolynomial(int initial_users) {
		for (int i = 0; i < initial_users; i++) {
			polynomial.push_back((rand() % int(rand() * MAX_RAND)) % modulo);
		}
		return;
	}

    // Get nth users' polynomial data
	int getPolynomial(int secret, int which) {
		long long temp = secret;
		for (int j = 0; j < polynomial.size(); j++) {
			temp += (long long)polynomial[j] * pow(which, j + 1);
		}
		return (int)(temp % modulo);
	}

	bool lagrangeCorrectPoints(vector<array<int, 2>> points) {
		for (int i = 0; i < points.size(); i++) {
			for (int j = i + 1; j < points.size(); j++) {
				if (points[i][0] == points[j][0]) {
					return false;
				}
			}
		}
		return true;
	}

	static double lagrangeBasisPolynomial(vector<array<int, 2>> points, int value, int x) {
		double multiplication = 1.0;
		for (int i = 0; i < points.size(); i++) {
			if (points[i][0] != value) {
				multiplication *= (double)(x - points[i][0]) / (value - points[i][0]);
			}
		}
		return multiplication;
	}

	static int lagrange(vector<array<int, 2>> points, int modulo) {
		double sum = 0;
		for (int i = 0; i < points.size(); i++) {
			sum += lagrangeBasisPolynomial(points, points[i][0], 0) * (double)points[i][1]; // Check the secret at 0
		}
		int temp = (int)round(sum) % modulo;
		if (temp < 0) {
			return temp + modulo;
		}
		return temp;
	}
};