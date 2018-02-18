// Small test program for turbo-buf
// g++ --std=c++14 test.cpp -o test.out

#include<iostream>
#include<cstdint>
#include<cstdio>
#include<vector>
#include<string>

// Ensure debug configuration for development
#define DEBUG_LOG 1	/* There are some detailed logs that happen to show only if this is set */
#define TBUF_ASSERT 1	/* There are some assertions we better use for development time */

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

	printf("Testing fio::LenString::safe_unescape(..)\n");
	std::string testUnescape = "Es\\cape the \\{reality\\}!\\\\\\\\ be 1337!"; // Rem.: Beware! From "///" it would become "/" with esc.
	std::string unesced = fio::LenString::safe_unescape('\\', testUnescape);
	printf("%s became: %s\n", testUnescape.c_str(), unesced.c_str());

	// Test some "real" functionality
	printf("Testing some real tbuf functionality...\n");
	// The second parameter means we are using crazy optimizations
	tbuf::Tree fruit(fin, true);

	printf("%s(%u)\n", fruit.root.core.name, fruit.root.core.data.asUint());
	printf("	%s(%u)\n", fruit.root.children[0].core.name, fruit.root.children[0].core.data.asUint());
	printf("		%s(%u)\n", fruit.root.children[0].children[0].core.name, fruit.root.children[0].children[0].core.data.asUint());
	printf("			%s(%s)\n", fruit.root.children[0].children[0].children[0].core.name, fruit.root.children[0].children[0].children[0].core.text);
	//printf("	%s(%u)\n", fruit.root.children[1].core.name, fruit.root.children[1].core.data.asUint());

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
	// Try ad-hoc polymorphism using prefixes
	tbuf::TreeQuery::fetch(fruit.root,
			std::vector<tbuf::LevelDescender>{
				// The third fruit_* should be chosen for descending
				tbuf::LevelDescender("fruit", 2, true),
				// and descending into its text node
				tbuf::LevelDescender(tbuf::SYM_STRING_NODE_STR, 0, true),
			},
			[&fetchTestOk] (tbuf::NodeCore &nc) {
				// should print out "pi" as text:
				printf("Found node with text: %s\n", nc.text);
				++fetchTestOk; // we should have found this...
			});
	tbuf::TreeQuery::fetch(fruit.root,
			{"notexistent", tbuf::SYM_STRING_NODE_CLASS_STR},
			[&fetchTestOk] (tbuf::NodeCore &nc) {
				// Nothing should get printed here: 
				printf("FIXME: either test input or code is broken! %s\n", nc.text);
				--fetchTestOk; // we should have not found this...
			});
	printf("...fetch test ok: %d\n", fetchTestOk);
	
	/* // Should fail with compile error:
	int test = 2;
	tbuf::Tree<int> fruit2(test);
	*/

	// Pretty-print the tree using dfs
	printf("Custom pretty-printing of the tree and testing DFS:\n");
	fruit.root.dfs_preorder([] (tbuf::NodeCore& nc, unsigned int depth, bool leaf){
			// Indentation
			for(unsigned int i = 0; i < depth; ++i) {
				printf("\t");
			}
			// Tree data
			if(leaf) {
				printf("*"); // Indicate leaf nodes somehow...
			}
			printf("%s(", nc.name);
			if(nc.text == nullptr) {
				// Normal node - show data as uint
				printf("%u)\n", nc.data.asUint());
			} else {
				// Text-node - show text
				printf("%s)\n", nc.text);
			}
	});

	// Writeback-pretty-printing the tree using dfs
	printf("Test writeOut - with pretty-printing:\n");
	fruit.root.writeOut();
	printf("Test writeOut - dense printing:\n");
	fruit.root.writeOut(stdout, false);

	printf("Test node addition...\n");
	tbuf::Node& text1 = fruit.addTextNode(fruit.root, "Runtime-added test data 1");
	fruit.addTextNode(fruit.root, "Runtime-added test data 2", "test2");
	fruit.addNormalNode(fruit.root, "FFAA0014", "test4");
	fruit.addDuplicate(fruit.root, text1);
	tbuf::Node& data1 = fruit.addNormalNode(fruit.root, "FFAA0013", "test3");
	// This would not work instead of the above as auto will be Node and not Node& sadly!!!
	// This can cause havok and mayhem if one does not know so some test code is added below uncommented.
	//auto data1 = fruit.addNormalNode(fruit.root, "FFAA0013", "test3");
	fruit.addDuplicate(data1, text1);
	//printf("data1.children.size(): %d\n", data1.children.size());
	//tbuf::Node& lastChild = fruit.root.children[fruit.root.children.size()-1];
	printf("root.lastChild(%s).children.size(): %d\n", lastChild.core.name, lastChild.children.size());
	printf("Test writeOut - after node additions (pretty-printing):\n");
	fruit.root.writeOut();

	printf("End of testing\n");
}
