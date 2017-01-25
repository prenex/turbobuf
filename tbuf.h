#ifndef TURBO_BUF_H
#define TURBO_BUF_H

#include<fstream>
#include<cstdio>

namespace tbuf {

/** A string of characters represented as a pointer to the start and length. There might be no zero terminator!!! */
struct LenString {
	unsigned int length;
	char* startPtr;
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
	 * Calling this function without advancing the head from the beginning is undefined behaviour!
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
};

// This is designed to be as fast as it can be by using a lot of inlining and tricks
// Also this implementation is "resetable" so provides reusable objects when needed.
/**
 * Input handler that is reading from memory or from files by reading them into the memory as a whole.
 * Works well for in-memory data and provides very fast operations for fast for small files too.
 */
class FastInput : Input {
private:
	int length;	// -1 on errors, otherwise the length
	char* buffer;	// the data of the file - read in all!
	char* head;	// reading head pointer
	bool ownsBuffer;// tells if the object owns its buffer or not
public:
	/**
	 * Create a fast-input handler using the data already in the memory.
	 * (!) The last character of the provided buffer need to be EOF for this to work (except length=0 cases)
	 * (!) Beware that this class might modify the underlying buffers when you ask for c_strs in the fast way.
	 * (!) This might result in having a really different scan after a reset, and changing memory with those unsafe operators!!!
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

	/**
	 * Destructive operator that changes the underlying buffers to create a null-terminated string from the LenString of yours.
	 * This input class provides a way to get unsafe null-terminated strings as it handles data directly in memory, however this
	 * should be used with care and deep understanding of the code...
	 *
	 * Also one should keep in the mind that this entangles the life-cycles of the returned pointers validity to the underlying buffer!
	 */
	static char* unsafe_get_str(LenString grabbedFromSeam){
	}

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

}
*/

}
#endif
