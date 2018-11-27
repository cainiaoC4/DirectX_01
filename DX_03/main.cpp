#include<d3dx10.h>
#include<iostream>

std::ostream& operator<<(std::ostream&os, D3DXVECTOR4&v) {

	os << "(" << v.x << "," << v.y << "," << v.z << "," << v.w << std::endl;

	return os;
}

std::ostream& operator<<(std::ostream&os, D3DXMATRIX& m)
{
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			os << m(i, j) <<" ";
		}
		os << std::endl;
	}

	return os;
}


int main() {
	D3DXMATRIX A(1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 4.0f, 0.0f,
		1.0f, 2.0f, 3.0f, 1.0f);

	D3DXMATRIX B;

	D3DXMatrixIdentity(&B);            //(Inoutput M) B成为单位矩阵 

	D3DXMATRIX C = A * B;

	D3DXMATRIX D, E, F;

	D3DXMatrixTranspose(&D, &A);      //(Output M^T ,Input M) 求转置矩阵

	D3DXMatrixInverse(&E, 0, &A);    //(output M^-1 ,FLOAT* ,Input M) 求逆矩阵

	F = A * E;

	D3DXVECTOR4 P(2.0f, 2.0f, 2.0f, 1.0f);
	D3DXVECTOR4 Q(2.0f, 2.0f, 2.0f, 0.0f);
	D3DXVECTOR4 R, S;
	D3DXVec4Transform(&R, &P, &A);        //(Output vM ,Input v,Input M)   (vec4)P* M
	D3DXVec4Transform(&S, &Q, &A);

	std::cout << "A= " << std::endl << A << std::endl;
	std::cout << "B= " << std::endl << B << std::endl;
	std::cout << "C=A*B=" << std::endl << C << std::endl;
	std::cout << "D=transpose(A)" << std::endl << D << std::endl;
	std::cout << "E=inverse(A)" << std::endl << E << std::endl;
	std::cout << "F=A*E=" << std::endl << F << std::endl;
	std::cout << "P=" << std::endl << P << std::endl;
	std::cout << "Q=" << std::endl << Q << std::endl;
	std::cout << "R=P*A=" << std::endl << R << std::endl;
	std::cout << "S=Q*A=" << std::endl << S << std::endl;



	
	return 0;
}