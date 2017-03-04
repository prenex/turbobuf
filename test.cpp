// Small test program for turbo-buf
// g++ --std=c++14 test.cpp -o test.out

#include<iostream>
#include<cstdint>
#include<cstdio>

#include"tbuf.h"
#include"fio.h"

void testFileInHandler();

int main(){
	// Various tests
	testFileInHandler();

	// Exit
	return 0;
}

void testFileInHandler(){
	printf("Trying to read from in.txt...\n");
	// Read data from input
	const char* inputFile = "in.txt";
	fio::FastInput fin(inputFile);

	printf("...file opened...\n");
	char data = fin.grabCurr();
	// Use data
	printf("...FileInHandler testing result(in.txt): %c\n", data);

	// TODO: Test "real" functionality
}
