#include<iostream>
#include<d3dx10.h>


using namespace std;


////D3DXVECTOR3 

ostream& operator<<(ostream& os, D3DXVECTOR3& v)
{
	os << "(" << v.x << "," << v.y << "," << v.z <<")"<< endl;

	return os;
}

int main()
{
	D3DXVECTOR3 u(1.0f, 2.0f, 3.0f);

	float x[3] = { -2.0f,1.0f,-3.0f };
	D3DXVECTOR3 v(x);                      //using constructor D3DXVECTOR3(CONST FLOAT*)

	D3DXVECTOR3 a, b, c, d, e;             //using constructor D3DXVECTOR3(){};

	a = u + v;                             //D3DXVECTOR3 operator+

	b = u - v;                             //D3DXVECTOR3 operator-

	c = u * 10;                            //D3DXVECTOR3 operator*

	float L = D3DXVec3Length(&u);          //||u||

	
	D3DXVec3Normalize(&d, &u);             //d=u/||u||             //may cause LNK2019 ,注意库是x64 还是x86的 

	float s = D3DXVec3Dot(&u, &v);         //s=u dot v

	D3DXVec3Cross(&e, &u, &v);             //e = u x v


	cout <<"u         =" << u << endl;
	cout << "v          =" << v << endl;
	cout << "a=u+v=      " << a << endl;
	cout << "b=u-v=     " << b << endl;
	cout << "c=u*10=    " << c << endl;
	cout << "d=u/||u||=   " << d << endl;
	cout << "e=uxv=    " << e << endl;
	cout << "L=||u||     =" << L << endl;
	cout << "s=u dot v =    " << s << endl;


	return 0;

}