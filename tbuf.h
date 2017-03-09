#ifndef TURBO_BUF_H
#define TURBO_BUF_H

#include<memory>
#include<vector>
#include<cstdio>
#include"fio.h"

namespace tbuf {

/** The special tree-node name that contains string-data */
const char SYM_STRING_NODE = '$';
const char SYM_OPEN_NODE = '{';
const char SYM_CLOSE_NODE = '}';
const char SYM_ESCAPE = '\\';	// The "\" is used for escaping in string nodes

/** Hex-data stream class */
class Hexes {
public:
	/**
	 * Contains the data hex string (can be empty length). It is represented in the "most significant bytes first" way!
	 */
	fio::LenString digits;

	/** Try to return the integral representation of this hex stream if possible */
	inline unsigned long long asIntegral() {
		// This could be optimized to do 4byte or 8byte processing in loop I think...
		long long result = 0;
		unsigned int len = digits.length;
		for(unsigned int i = 0; i < len; ++i) {
			result = (result << 4) + Hexes::hexValueOf(digits.startPtr[i]);
		}
	}

	/** Try to return the unsigned int representation of this hex stream if possible */
	inline unsigned int asUint() {
		// This could be optimized to do 4byte or 8byte processing in loop I think...
		unsigned int result = 0;
		unsigned int len = digits.length;
		for(unsigned int i = 0; i < len; ++i) {
			result = (result << 4) + Hexes::hexValueOf(digits.startPtr[i]);
		}
	}

	/** Converts a character of [0..9] or [A..F] to its [0..15] integer range */
	inline static char hexValueOf(char hex) {
		if('A' <= hex && hex <= 'F') {
			// Hexit
			return (hex - 'A') + 10;
		} else {
			// Digit
			return (hex - '0');
		}
	}

	/** Tells if a character is among ['0'..'9'] or ['A'..'F'] */
	inline static bool isHexCharacter(char c) {
		// TODO: check if there is a faster way for ascii using bit hackz...
		if(('A' <= c && c <= 'F') || ('0' <= c && c <= '9')) {
			return true;
		} else {
			return false;
		}
	}
};

/**
 * Defines possible node kinds
 */
enum NodeKind {
	/** An empty node indicating we cannot read anything */
	EMPTY = 0,
	/** The root-node - it is a special normal node */
	ROOT = 1,
	/** Just a normal node */
	NORM = 2,
	/** Text-containing ${str} nodes */
	TEXT = 3,
};

/**
 * The generic turbo-buf tree-node. Do not cache the values that are owned by the tree to avoid shooting your leg!
 */
struct Node {
	/** Defines the kind of this node */
	NodeKind nodeKind;
	/**
	 * Contains the data hex string (can be empty length)
	 * - DO NOT USE DANGEROUS AND DESTRUCTIVE OPERATIONS ON THIS - Underlying memory is OWNED BY THE TREE! Do not cache this!
	 */
	Hexes data;
	/**
	 * Contains the name of the node (different nodes might share the same name)
	 * - MEMORY IS OWNED BY THE TREE! Do not cache this!
	 */
	const char *name;
	/**
	 * Only contains a valid pointer if the node kind is TEXT when it contains the utf8 char string. Otherwise nullptr.
	 * Also a nullptr if the node text is empty (so instead of "", we use nullptr).
	 * - MEMORY IS OWNED BY THE TREE! Do not cache this!
	 */
	const char *text;

	/**
	 * Points to our parent - can be nullptr for implicit root nodes
	 * - MEMORY IS OWNED BY THE TREE! Do not cache this!
	 */
	Node* parent;

	/** The child nodes (if any) */
	std::vector<Node> children;
};

template<class InputSubClass>
class Tree {
public:
	/** Name for implicit root nodes */
	const char *rootNodeName = "/";	// This name is special as it can be '/' only for the root - see tbnf description!
	const char *textNodeName = "$"; // This name is special. We use this directly instead of pointing into the buffers...

	/** The root node for this tree */
	Node root;

	/**
	 * Create tree by parsing input. Takes ownership of input so that we can parse with optimizations
	 */
	Tree(InputSubClass &input) {
printf("TESTERBESTER-0\n");
		// These are only here to ensure type safety
		// in our case of template usage...
		fio::Input *testSubClassing = new InputSubClass();
		delete testSubClassing;	// Should be fast!

		// Parse
printf("TESTERBESTER-1\n");
		
		// Check if we have any input to parse
		if(input.grabCurr() == EOF) {
			// Empty input file, return empty root
			root = Node{NodeKind::ROOT, Hexes{fio::LenString{0,nullptr}}, rootNodeName, nullptr, nullptr, std::vector<Node>()};
		} else {
			// Properly parse the whole input as a tree
			// Parse hexes for the root node
			Hexes rootHexes = parseHexes(input);
			// Create the root node with empty child lists
			root = Node {NodeKind::ROOT, rootHexes, rootNodeName, nullptr, nullptr, std::vector<Node>()};
printf("TESTERBESTER-2\n");
			// Fill-in the children while parsing nodes with tree-walking
			parseNodes(input, root);
		}
	}
private:
	/**
	 * Advances input until it is a hex characted and parse.
	 * The current of input will point after the first non-hex character after this operation...
	 */
	inline Hexes parseHexes(InputSubClass &input) {
		if(!Hexes::isHexCharacter(input.grabCurr())) {
			// No hexes at current position
			return Hexes {{0, nullptr}};
		} else {
			// Hexes at current position
			void* hexSeam = input.markSeam();
			while(Hexes::isHexCharacter(input.grabCurr())) {
				input.advance();
			}
			fio::LenString digits = input.grabFromSeamToLast(hexSeam);
			return Hexes { digits };
		}
	}

	/** Parse all child nodes of the root after parsing hexes of root */
	inline void parseNodes(InputSubClass &input, Node &parent){
		// Parse from the very start point after the hexes of the root!
		// (Non-recursive tree-walking in a depth first approach)
		Node* currentParent = &parent;
		while(nullptr != (currentParent = parseNode(input, currentParent)));
	}

	/**
	 * Parse one node and put it as the child node of the referred parent.
	 * returns the pointer to the new parent (we do a depth first tree-walking) or nullptr if we reached EOF!
	 * The new parent can be the child of the parent if we can go deeper, or the parent if we keep the level or
	 * the parent of the parent if we see that have gotten the parents closing parentheses...
	 */
	inline Node* parseNode(InputSubClass &input, Node *parent) {
		// Sanity check
		if(parent == nullptr) {
			return nullptr;
		}

printf("TESTER-BESTER-CURRCAR: %c\n", input.grabCurr());

		// Try to parse a node which will be saved into the parent
		if(input.grabCurr() == EOF) {
			// Finished parsing
			return nullptr;
		} else if(input.grabCurr() == SYM_STRING_NODE) {
			// Parse '$' symbol tag with string inside - this is always a leaf!
			// advance onto the '{' by skipping anything in-between
			// this way we can later add various types of string nodes by adding
			// a type name after the '$' in the protocol!
			while(input.grabCurr() != SYM_OPEN_NODE) {
				// Input sanity check
				if(input.grabCurr() == EOF) {
					// Completely depleted input: finish parsing
					// happens on badly formatted input...
					return nullptr;
				} else {
					// advance to find the node opening
					input.advance();
				}
			}
			// Let us try to find the contents then...
			fio::LenString content;
			// Go after the '{' - we are now in the inside of the node
			input.advance();
			// This will hold the current character
			char current = input.grabCurr();
			// We mark a seam so that we can accumulate input
			// this should work even if current became EOF immediately!
			void* seamHandle = input.markSeam();
			// Initialize closes flag. EOF surely closes (should not happen though)
			// and also '}' surely closes as no escaping was possible until yet...
			bool nodeClosed = ((current == EOF) || (current == SYM_CLOSE_NODE));
			// Loop and parse the text contents of this node
			while(!nodeClosed) {
				// See if this node is already closed or not
				// here "current" still refers to the last character...
				bool escaped = (current == SYM_ESCAPE);
				input.advance();
				current = input.grabCurr();
				if((current == SYM_CLOSE_NODE) && (!escaped)) {
					// Found the end of the inside of the node
					content = input.grabFromSeamToLast(seamHandle);
					nodeClosed = true;
				}
			}

			// Get the text of this node
			char* text = nullptr;
			if(input.isSupportingDangerousDestructiveOperations()) {
				// Create null terminated c_str from the LenString
				text = content.dangerous_destructive_unsafe_get_zeroterminator_added_cstr();
printf("KULAKVA: %s\n", text);
				// Support escaping - at least for the '}' character
				content.dangerous_destructive_unsafe_escape_in_place(SYM_ESCAPE);
			} else {
				// FIXME: Support escaping - at least for the '}' character
				// FIXME: Create null terminated c_str from the LenString
				fprintf(stderr, "FIXME: implement slower and safe operations!");
			}

			// Add this new node as our children to the parent
			parent->children.push_back(Node{
					NodeKind::TEXT,
					Hexes {},
					textNodeName,
					text,
					parent,
					std::vector<Node> {}
			});
			
			// Always advance the input when it is not the EOF already!
			input.advance();

			// Because the text-only nodes are always leaves
			// the parents stays as it was
			return parent;
		} else if(input.grabCurr() == SYM_CLOSE_NODE) {
			// Parsed the '}' closing symbol
			// Always advance the input when it is not the EOF already!
			input.advance();
			// We should go up one level in the tree walking loop!
			// The only thing we need to do is thus to go upwards
			// if the current parent still had any parent
			if(parent->parent != nullptr) {
				// Go up one level
				return parent->parent;
			} else {
				// Already at top level
				// just stay there and wait for EOF...
				return parent;
			}
		} else {
			// TODO: We are parsing a normal node and the read head is on the first letter of the name
			// Parse node name and node body (the hexes for it)!
			// Set the newly parsed node as the new current parent by returning it!
			// Always advance the input when it is not the EOF already!
			input.advance();

			// FIXME: this is no good
			return parent;
		}
	}
};

} // tbuf namespace ends here
#endif // TURBO_BUF_H
