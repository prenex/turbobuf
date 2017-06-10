// fio.h: The fast and simple input(-output) handler / scanner as a header-only library. Originally made for tbuf.

#ifndef _FAST_IO_H
#define _FAST_IO_H

#include<fstream>
#include<cstdio>
#include<vector>
#include<algorithm>
#include"fio_data.h"

// The I/O part of fio
namespace fio {

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
	 *
	 * When using tbuf.h basically you let them put terminating zero characters on the address _AFTER_ subtree identifier
	 * LenString char* sequences and the inner sequences of the ${...} kind of string utf8 nodes. In the earlier mentioned
	 * case the terminator overrides the opening '{' and in the latter case it will override the closing '}' 
	 * in memory after processing. Because of the structure. Other users of this library might do other optimizations but
	 * they always try to close LenString or to change the original memory in the middle of the string (for ex. handle escaping).
	 * Some implementation might allow users to do these tricks, but others (like maybe a memory mapped file) cannot let them do this!
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
class FastInput : public Input {
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
			((seamHandle != nullptr) && (head > (char*)seamHandle)) ? (unsigned int)(head - (char*)seamHandle) : 0,
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

} // fio namespace ends

#endif // _FAST_IO_H
