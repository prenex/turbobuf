#ifndef TURBO_BUF_H
#define TURBO_BUF_H

#include<fstream>
#include<cstdio>
#include<vector>

namespace tbuf {

/** A string of characters represented as a pointer to the start and length. There might be no zero terminator!!! */
struct LenString {
	unsigned int length;
	char* startPtr;

	/**
	 * Destructive operator that changes the underlying buffers to create a null-terminated string from the LenString of yours.
	 * When length == 0, this operation returns a nullptr and does not change any bytes anywhere - otherwise a byte is always
	 * changed after the original string. Really dangerous operator and should be used only if there is a space for the
	 * extra one byte after the string!
	 *
	 * Also one should keep in the mind that this entangles the life-cycles of the returned pointers validity to the underlying buffer!
	 */
	char* dangerous_destructive_unsafe_get_zeroterminator_added_str() {
		if(length == 0) {
			return nullptr;
		} else {
			// Add null terminator
			*(startPtr + length) = 0;
			return startPtr;
		}
	}

	/**
	 * Returns a new corresponding std::string
	 */
	std::string get_str() {
		if(length == 0) {
			return "";
		} else {
			// Copy character array contents
			std::vector<char> charVec(length + 1);
			for(int i = 0; i < length; ++i) {
				charVec[i] = startPtr[i];
			}
			// ensure null terminator
			charVec[length] = 0;
			// Create a string from this character array
			// and return this copy.
			return std::string(&(charVec[0]));
		}
	}
};

/**
 * Base-class for all generic input handlers
 * and the sample implementations below.
 *
 * Because most methods need to be extremely fast
 * we cannot use vtable and virtual functions.
 * To be able to choose the input method, we
 * use templates in the classes that build upon
 * the input handlers. We try our best to ensure
 * they are inheriting from Input, but that is
 * not really helpful to ensure they implement
 * its functionality properly. This is the best
 * that we can do while keeping speed.
 */
class Input {
public:
	/** Empty parameterless constructor - you need to implement this too! */
	Input() { }

	/**
	 * Mark a seam at current head position
	 * We can get the whole array of characters from this seam point 
	 * from now on - up to the head position anytime!
	 *
	 * Returns a pointer/handle to the marked seam which might be always nullptr if the Input does not support multiple seam markers!
	 */
	void* markSeam() { }
	/**
	 * Returns the current character of the input
	 * The method returns EOF when we have already reached the end of stream!
	 */
	char grabCurr() { }
	/**
	 * Returns the character right before the current head position.
	 * Calling this function without advancing the head from the beginning is undefined behaviour (it can even crash!)
	 */
	char grabLast() { }

	/** Advance the reading position. You should not advance behind grabCurr() returning EOF as that is undefined! */
	void advance() { }

	/**
	 * Grab all characters of the input from the point defined by the given seam handle until the current head.
	 * The resulting length+char* includes the characted right below the head - the one you can get with grabCurr!
	 */
	LenString grabFromSeamToCurr(void* seamHandle) { }

	/**
	 * Grab all characters of the input from the point defined by the given seam handle until the character BEFORE the current head.
	 * The resulting length+char* EXCLUDES the characted right below the head - but has the one you can get with grabLast or is empty!
	 */
	LenString grabFromSeamToLast(void* seamHandle) { }

	/**
	 * Tells the user if the Input sub-class is supporting dangerous operations that modify underlying char sequences and such or not!
	 * When this returns true, the using code might do crazy optimizations so beware!
	 */
	bool isSupportingDangerousDestructiveOperations() { }
};

// This is designed to be as fast as it can be by using a lot of inlining and tricks
// Also this implementation is "resetable" so provides reusable objects when needed.
/**
 * Input handler that is reading from memory or from files by reading them into the memory as a whole.
 * Works well for in-memory data and provides very fast operations for fast for small files too.
 * This input class provides a way to get null-terminated strings with unsafe_get_str as it handles
 * data directly in memory, however this should be used with care and deep understanding of the code!
 */
class FastInput : Input {
private:
	int length;	// -1 on errors, otherwise the length
	char* buffer;	// the data of the file - read in all!
	char* head;	// reading head pointer
	bool ownsBuffer;// tells if the object owns its buffer or not
public:
	FastInput() {
		length = 0;		// This might help others not use us
		buffer = nullptr;	// This might help others not use us
		head = nullptr;		// This might help others not use us
		ownsBuffer = false;	// This ensures no delete happens
	}

	/**
	 * Create a fast-input handler using the data already in the memory.
	 * (!) The last character of the provided buffer need to be EOF for this to work (except length=0 cases)
	 * (!) Beware that the returned LenString's might modify the underlying buffers when you ask for c_strs in the fast way.
	 * (!) This might result in having a really different scan after a reset when changing memory with those unsafe operators!!!
	 */
	FastInput(const int buf_len, char* buffer, bool ownsProvidedBufferMemory = true) {
		// We just copy pointers and provided data!
		length = buf_len;
		buffer = buffer;
		head = buffer;
		// They tell us if we own the provided buffer memory or not
		// so that we delete it in destructor or not!
		ownsBuffer = ownsProvidedBufferMemory;
	}

	/** Create a fast-input handler that reads on the given file */
	FastInput(const char* inFileName) {
		std::ifstream t;
		t.open(inFileName);		// open input file
		t.seekg(0, std::ios::end);	// go to the end
		length = t.tellg();		// report location (this is the length)
		t.seekg(0, std::ios::beg);	// go back to the beginning
		buffer = nullptr;
		// Read the whole file into the generated buffer
		if(length > 0) {
			// Read the file as-is into the memory
			buffer = new char[length + 1];	// allocate memory for a buffer of appropriate dimension + 1 (the EOF)
			ownsBuffer = true;		// We own the buffer as you see I create it just above!
			t.read(buffer, length);		// read the whole file into the buffer
			// Append extra EOF character at the end
			buffer[length] = EOF;
		}
		// Reset reading head
		head = buffer;
		// Close file handle (we read the whole so we do not need it anymore)
		t.close();
	}

	inline char grabCurr() {
		return *head;
	}

	inline char grabLast() {
		// This might crash when we grab before the buffer!
		return *(head - 1);
	}

	inline void* markSeam() {
		return (void*)head;
	}

	inline void advance() {
		++head;
	}

	inline LenString grabFromSeamToCurr(void* seamHandle) {
		return LenString {
			(head >= (char*)seamHandle) && length > 0 ? (unsigned int)(head - (char*)seamHandle) + 1 : 0,
			(char*)seamHandle
		};
	}

	inline LenString grabFromSeamToLast(void* seamHandle) {
		return LenString {
			(head > (char*)seamHandle) ? (unsigned int)(head - (char*)seamHandle) : 0,
			(char*)seamHandle
		};
	}

	/**
	 * (!) Beware that this class might modify the underlying buffers when you ask for c_strs in the fast way.
	 * (!) This might result in having a really different scan after a reset, if those unsafe operators have been used!
	 */
	inline void reset() {
		// Go to the start
		head = buffer;
	}

	/** Indeed we are supporting these dangerous operations when needed! */
	inline bool isSupportingDangerousDestructiveOperations() { return true; }

	~FastInput() {
		if(buffer != nullptr && ownsBuffer) {
			delete buffer;
		}
	}
};

// An example class that shows how to use the input interface properly: no overhead of virtual methods, but code can choose implementation!
/*
template<class InputSubClass>
class InputUser {
public:
	...
}
*/

/**
 * Defines possible node kinds
 */
enum NodeKind {
	ROOT = 0,
	NORM = 1,
	TEXT = 2,
};

/**
 * The generic turbo-buf tree-node
 */
class Node {
public:
	/** Defines the kind of this node */
	NodeKind nodeKind;
	/**
	 * Contains the data hex string (can be empty length)
	 * - DO NOT USE DANGEROUS AND DESTRUCTIVE OPERATIONS ON THIS - OWNED BY THE TREE!
	 */
	LenString data;
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
	/** The child nodes (if any) */
	std::vector<Node> children;
};

template<class InputSubClass>
class Tree {
public:
	Node root;

	/**
	 * Create tree by parsing input. Takes ownership of input so that we can parse with optimizations
	 */
	Tree(std::unique_ptr<InputSubClass> input) {
		// These are only here to ensure type safety
		// in our case of template usage...
		Input *testSubClassing = new InputSubClass();
		delete testSubClassing;	// Should be fast!

		// TODO: Parse
	}
private:
	std::unique_ptr<InputSubClass> source;
}

}
#endif
