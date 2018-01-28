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

static unsigned int lastDepth = 0; // Needed for putting up the closing '}' chars!
// Rem.: Better make this inlined so that the optimizer can take out the ifs for pretty-printing!
inline void writeBackFun(tbuf::NodeCore& nc, unsigned int depth, bool prettyPrint = true){
	// Possibly close earlier node (see that this handles root properly too!)
	if(prettyPrint && (depth > 0)) printf("\n");
	while((depth != 0) && (lastDepth >= depth)) {
		//printf("::%d:%d::", lastDepth, depth);
		if(prettyPrint && (lastDepth > 0)) {
			for(unsigned int i = 0; i < lastDepth-1; ++i) {
				printf("\t");
			}
		}
		printf("}");
		if(prettyPrint) printf("\n");
		--lastDepth;
	}
	// Indentation
	if(prettyPrint && (depth > 0)) {
		for(unsigned int i = 0; i < depth-1; ++i) {
			printf("\t");
		}
	}
	// Tree data
	// name is only needed if the depth is non-zero
	if(depth > 0) { printf("%s{", nc.name); }
	if(nc.text == nullptr) {
		// Normal node - show data as uint
		// (if there is any data)
		if(!nc.data.isEmpty()) {
			printf("%X", nc.data.asUint());
		}
	} else {
		// Text-node - show text
		printf("%s", nc.text);
	}
	lastDepth = depth;
}

// Visitors for writing out the nodes returned by a DFS
inline void writeBackPrettyVisitor(tbuf::NodeCore& nc, unsigned int depth){
	writeBackFun(nc, depth, true); // Pretty-printing visitor
}
inline void writeBackVisitor(tbuf::NodeCore& nc, unsigned int depth){
	writeBackFun(nc, depth, false); // No pretty-printing visitor
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
	fruit.root.dfs_preorder([] (tbuf::NodeCore& nc, unsigned int depth){
			// Indentation
			for(unsigned int i = 0; i < depth; ++i) {
				printf("\t");
			}
			// Tree data
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
	printf("Writeback-pretty-printing tree and testing DFS:\n");

	bool prettyPrint = true;

	if(prettyPrint) {
		fruit.root.dfs_preorder(writeBackPrettyVisitor);
	} else {
		fruit.root.dfs_preorder(writeBackVisitor);
	}
	while(lastDepth > 0) {
		//printf("::%d:%d::", lastDepth, depth);
		if(prettyPrint && (lastDepth > 0)) {
			for(unsigned int i = 0; i < lastDepth-1; ++i) {
				printf("\t");
			}
		}
		printf("}");
		if(prettyPrint) printf("\n");
		--lastDepth;
	}


	printf("End of testing\n");
}
