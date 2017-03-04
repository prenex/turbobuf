#ifndef TURBO_BUF_H
#define TURBO_BUF_H

#include<memory>
#include<vector>
#include<cstdio>
#include"fio.h"

namespace tbuf {

/** Hex-data stream class */
class Hexes {
public:
	/**
	 * Contains the data hex string (can be empty length). It is represented in the "most significant bytes first" way!
	 */
	fio::LenString digits;

	/** Try to return the integral representation of this hex stream if possible */
	inline unsigned long long asIntegral() {
		// TODO this could be optimized to do 4byte or 8byte processing in loop I think...
		long long result = 0;
		unsigned int len = digits.length;
		for(unsigned int i = 0; i < len; ++i) {
			result = (result << 4) + hexValueOf(digits.startPtr[i]);
		}
	}

	/** Try to return the unsigned int representation of this hex stream if possible */
	inline unsigned int asUint() {
		// TODO this could be optimized to do 4byte or 8byte processing in loop I think...
		unsigned int result = 0;
		unsigned int len = digits.length;
		for(unsigned int i = 0; i < len; ++i) {
			result = (result << 4) + hexValueOf(digits.startPtr[i]);
		}
	}

	/** Converts a character of [0..9] or [A..F] to its [0..15] integer range */
	inline char hexValueOf(char hex) {
		if('A' <= hex && hex <= 'F') {
			// Hexit
			return (hex - 'A') + 10;
		} else {
			// Digit
			return (hex - '0');
		}
	}

	/** Tells if a character is among ['0'..'9'] or ['A'..'F'] */
	inline bool isHexCharacter(char c) {
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
class Node {
public:
	/** Defines the kind of this node */
	NodeKind nodeKind;
	/**
	 * Contains the data hex string (can be empty length)
	 * - DO NOT USE DANGEROUS AND DESTRUCTIVE OPERATIONS ON THIS - OWNED BY THE TREE!
	 */
	Hexes data;
	/**
	 * Contains the name of the node (different nodes might share the same name)
	 * - MEMORY IS OWNED BY THE TREE!
	 */
	char *name;
	/**
	 * Only contains a valid pointer if the node kind is TEXT when it contains the utf8 char string. Otherwise nullptr.
	 * - MEMORY IS OWNED BY THE TREE!
	 */
	char *text;

	/**
	 * Points to our parent - can be nullptr for implicit root nodes
	 * - MEMORY IS OWNED BY THE TREE!
	 */
	Node* parent;

	/** The child nodes (if any) */
	std::vector<Node> children;
};

enum class ParseState {
	WAIT_HEX_OR_NODE,
	STRING,

};

template<class InputSubClass>
class Tree {
public:
	/** Name for implicit root nodes - your root node might get named differently if it is not top level! */
	const char *rootNodeName = "/";	// This name is special as it can be '/' only for the root - see tbnf description!

	/** The root node for this tree */
	Node root;

	/**
	 * Create tree by parsing input. Takes ownership of input so that we can parse with optimizations
	 */
	Tree(InputSubClass &input) {
		// These are only here to ensure type safety
		// in our case of template usage...
		fio::Input *testSubClassing = new InputSubClass();
		delete testSubClassing;	// Should be fast!

		// Parse
		
		// Check if we have any input to parse
		if(input.grabCurr() == EOF) {
			// Empty input file, return empty root
			root = {NodeKind::ROOT, Hexes{fio::LenString{0,nullptr}}, rootNodeName, nullptr, nullptr, std::vector<Node>()};
		} else {
			// TODO: properly parse
			Hexes hexes;
			// We have something to parse
			if(isHexCharacter(input.grabCurr())) {
				// Parse hexes
				hexes = parseHexes(input);
			}
		}
	}
private:
	Hexes parseHexes(InputSubClass &input) {
		void* hexSeam = input.markSeam();
		while(isHexCharacter(input.grabCurr())) {
			input.advance();
		}
		fio::LenString digits = input.grabFromSeamToLast();
		return Hexes { digits };
	}
};

} // tbuf namespace ends here
#endif
