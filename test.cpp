// Small test program for turbo-buf
// g++ --std=c++14 test.cpp -o test.out

#include<iostream>
#include<cstdint>
#include<cstdio>

// Ensure debug logging is in effect...
#define DEBUG_LOG 1

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

	// TODO: Test some "real" functionality
	printf("Testing some real tbuf functionality...\n");
	tbuf::Tree<fio::FastInput> fruit(fin);

	printf("%s(%u)\n", fruit.root.name, fruit.root.data.asUint());
	printf("	%s(%u)\n", fruit.root.children[0].name, fruit.root.children[0].data.asUint());
	printf("		%s(%s)\n", fruit.root.children[0].children[0].name, fruit.root.children[0].children[0].text);
	printf("	%s(%u)\n", fruit.root.children[1].name, fruit.root.children[1].data.asUint());
	
	/* // Should fail with compile error:
	int test = 2;
	tbuf::Tree<int> fruit2(test);
	*/

	printf("End of testing\n");
}
