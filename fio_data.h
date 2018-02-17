// fio_data.h: The data structures only for the fast and simple input(-output) handler / scanner as a header-only library.
// Originally made for tbuf.
// Rem.: This is separated to aid fast compilation while we keep fio as a header-only library!

#ifndef _FAST_IO_DATA_H
#define _FAST_IO_DATA_H

// The data-only part of fio
namespace fio {

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
	inline char* dangerous_destructive_unsafe_get_zeroterminator_added_cstr() {
		if(length == 0) {
			return nullptr;
		} else {
			// Add null terminator
			*(startPtr + length) = 0;
			return startPtr;
		}
	}

	/**
	 * Destructive operator that changes the underlying buffers in memory
	 * by eliminating escaped characters. This is a dumb operation that basically
	 * just removes the _FIRST_ escape character. If the escape char is '\' then:
	 * -- "\\" becomes "\",0
	 * -- "al\ma" becomes "alma",0
	 * -- "\\\" becomes "\",0,'\'
	 * -- "\\\\" becomes "\\",0,'\'
	 * -- "al\\\ma" becomes "al\ma",0,'a'
	 *
	 * Basically what the escaping is doing is that every character after the
	 * escapeChar need to be taken literally. If you are parsing in that way
	 * that you ignore escaped closing characters while accumulating characters
	 * this could be really handy!
	 */
	inline void dangerous_destructive_unsafe_unescape_in_place(char escapeChar) {
		bool escaped = false;	// true right after an escape char
		int i; // i - Read head; j<=i. Always step incrementally and move through the array of chars
		int j; // j - Write head: just go over the array. Step like i, but in case of escapes step back too
		for(i = 0, j = 0; (i < length) && (j < length); ++i, ++j) {
			char current = startPtr[i];
			if(escaped || (escapeChar != current)) {
				// Normal character, or escaped escape char - just copy to write head
				startPtr[j] = current;
				// Escaping will stop if there was any
				escaped = false;
			} else {
				// Unescaped escape character
				escaped = true;
				// This decrementation will make j stay in place!
				// while not writing anything will miss out this
				// character from the output completely!
				--j;
			}
		}
		// Add the new zero terminator!
		startPtr[j] = 0;
	}

	/**
	 * Escape simply by eliminating escape-characters. This is a dumb operation that basically
	 * just removes the _FIRST_ escape character. If the escape char is '\' then:
	 * -- "\\" becomes "\",0
	 * -- "al\ma" becomes "alma",0
	 * -- "\\\" becomes "\",0,'\'
	 * -- "\\\\" becomes "\\",0,'\'
	 * -- "al\\\ma" becomes "al\ma",0,'a'
	 *
	 * Basically what the escaping is doing is that every character after the
	 * escapeChar need to be taken literally. If you are parsing in that way
	 * that you ignore escaped closing characters while accumulating characters
	 * this could be really handy!
	 */
	inline static std::string safe_unescape(char escapeChar, std::string src) {
		// Create a big-enough string, padded with zeroes (the result can be only this big or smaller)
		std::string ret(src.length(), (char)0);
		bool escaped = false;	// true right after an escape char
		int i; // i - Read head; j<=i. Always step incrementally and move through the string
		int j; // j - Write head: just go over the string. Step like i, but in case of escapes step back too
		for(i = 0, j = 0; (i < src.length()) && (j < src.length()); ++i, ++j) {
			char current = src[i];
			if(escaped || (escapeChar != current)) {
				// Normal character, or escaped escape char - just copy to write head
				ret[j] = current;
				// Escaping will stop if there was any
				escaped = false;
			} else {
				// Unescaped escape character
				escaped = true;
				// This decrementation will make j stay in place!
				// while not writing anything will miss out this
				// character from the output completely!
				--j;
			}
		}
		// Return the created string
		return ret;
	}

	/**
	 * Returns a new corresponding std::string
	 */
	inline std::string get_str() {
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
			return std::move(std::string(&(charVec[0])));
		}
	}
};

} // fio namespace ends

#endif // _FAST_IO_DATA_H
