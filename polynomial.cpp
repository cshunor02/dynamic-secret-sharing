#include <vector>
#include <array>

using namespace std;

class Polynomial {
public:
	// Default constructor
	Polynomial() {}

	// To calculate the Lagrange basis polynomial in point x
	static double lagrangeBasisPolynomial(vector<array<int, 2>> points, int value, int x) {
		double multiplication = 1.0;
		for (int i = 0; i < points.size(); ++i) {
			if (points[i][0] != value) {
				multiplication *= (double)(x - points[i][0]) / (value - points[i][0]);
			}
		}
		return multiplication;
	}
};