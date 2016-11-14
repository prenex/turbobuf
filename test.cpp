// Various little experiments
// g++ --std=c++14 test.cpp -o test.out

#include<iostream>
#include<cstdint>

#include"tbuf.h"

int main(){
	// 4 bytes - enough for all the unicode!!!
	std::cout << "sizeof(wchar_t): " << sizeof(wchar_t) << std::endl;
	std::cout << "sizeof(uint8_t): " << sizeof(uint8_t) << std::endl;

	return 0;
}
