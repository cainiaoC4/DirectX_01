#include<iostream>
#include<d3dx10.h>

//float may cause errors

// to prevent errors try Equals Fuc
const float EPSILON = 0.001f;
bool Equals(float lhs,float rhs) {

	return fabs(lhs - rhs) < EPSILON ? true : false;
}


int main() {

	std::cout.precision(8);

	D3DXVECTOR3 u(1.0f,1.0f,1.0f);

	D3DXVec3Normalize(&u, &u);

	float LU = D3DXVec3Length(&u);

	std::cout << LU << std::endl;
	if (LU == 1.0f) {

		std::cout << "Length 1" << std::endl;
	}
	else {
		std::cout << "Length not 1" << std::endl;
	}

	float powLU = powf(LU, 1.0e6f);
	std::cout << "LU^(10^6) = " <<powLU<< std::endl;

	return 0;
}