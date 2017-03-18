// Small test program for turbo-buf
// g++ --std=c++14 test.cpp -o test.out

#include<iostream>
#include<cstdint>
#include<cstdio>
#include<vector>

// Ensure debug logging is in effect...
#define DEBUG_LOG 1

#include"tbuf.h"
#include"fio.h"

void testTbuf();

int main(){
	// Various tests
	testTbuf();

	// Exit
	return 0;
}

void testTbuf(){
	printf("Trying to read from in.txt...\n");
	// Read data from input
	const char* inputFile = "in.txt";
	fio::FastInput fin(inputFile);

	printf("...file opened...\n");
	char data = fin.grabCurr();
	// Use data
	printf("...FileInHandler testing result(in.txt): %c\n", data);

	// Test some "real" functionality
	printf("Testing some real tbuf functionality...\n");
	tbuf::Tree<fio::FastInput> fruit(fin);

	printf("%s(%u)\n", fruit.root.core.name, fruit.root.core.data.asUint());
	printf("	%s(%u)\n", fruit.root.children[0].core.name, fruit.root.children[0].core.data.asUint());
	printf("		%s(%s)\n", fruit.root.children[0].children[0].core.name, fruit.root.children[0].children[0].core.text);
	printf("	%s(%u)\n", fruit.root.children[1].core.name, fruit.root.children[1].core.data.asUint());

	printf("Fetch testing...\n");
	int fetchTestOk = 0;
	tbuf::TreeQuery::fetch(fruit.root,
			std::vector<tbuf::LevelDescender>{
				tbuf::LevelDescender("egy"),
				tbuf::LevelDescender("ketto"),
				tbuf::LevelDescender("harom"),
			},
			[&fetchTestOk] (tbuf::NodeCore &nc) {
				printf("Found node with data: %u\n", nc.data.asUint());
				++fetchTestOk; // we should have found this...
			});
	tbuf::TreeQuery::fetch(fruit.root,
			std::vector<tbuf::LevelDescender>{
				tbuf::LevelDescender("egy"),
				tbuf::LevelDescender("ketto"),
				tbuf::LevelDescender("harom"),
			},
			[&fetchTestOk] (tbuf::NodeCore &nc) {
				printf("Found node with data: %u\n", nc.data.asUint());
				++fetchTestOk; // we should have found this...
			});
	tbuf::TreeQuery::fetch(fruit.root,
			{"hololo", tbuf::SYM_STRING_NODE_STR},
			[&fetchTestOk] (tbuf::NodeCore &nc) {
				printf("Found node with text: %s\n", nc.text);
				++fetchTestOk; // we should have found this...
			});
	printf("...fetch test ok: %d\n", fetchTestOk);
	
	/* // Should fail with compile error:
	int test = 2;
	tbuf::Tree<int> fruit2(test);
	*/

	printf("End of testing\n");
}
