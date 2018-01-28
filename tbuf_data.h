#ifndef TURBO_BUF_DATA_H
#define TURBO_BUF_DATA_H

#include<memory>
#include<vector>
#include<cstdio>
#include<cstring>
#include<cctype>
#include<functional>
#include<initializer_list>
#include<unordered_set>
#include"fio_data.h"

// Uncomment this if we want to see the debug logging
// Or even better: define this before including us...
//#define DEBUG_LOG 1

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
		// This could be optimized to do 4byte or 8byte processing in loop I think...
		long long result = 0;
		unsigned int len = digits.length;
		for(unsigned int i = 0; i < len; ++i) {
			result = (result << 4) + Hexes::hexValueOf(digits.startPtr[i]);
		}
		return result;
	}

	/** Try to return the unsigned int representation of this hex stream if possible */
	inline unsigned int asUint() {
		// This could be optimized to do 4byte or 8byte processing in loop I think...
		unsigned int result = 0;
		unsigned int len = digits.length;
		for(unsigned int i = 0; i < len; ++i) {
			result = (result << 4) + Hexes::hexValueOf(digits.startPtr[i]);
		}
		return result;
	}

	inline bool isEmpty() {
		return (digits.length == 0);
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

	/** Return the empty hexes */
	inline static Hexes EMPTY_HEXES() {
	       return	{{0, nullptr}};
	}
};

/**
 * Defines possible node kinds
 */
enum class NodeKind {
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
 * The generic core of a turbo-buf tree-node. Do not cache the values that are owned by the tbuf trees to avoid shooting your leg!
 * This is what the user should see when he is providing callbacks to walk over the tree or get data.
 */
struct NodeCore {
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
};

} // end of namespace tbuf

#endif // TURBO_BUF_DATA_H
