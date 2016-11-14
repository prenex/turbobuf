#ifdef TURBO_BUF_H
#define TURBO_BUF_H

namespace tbuf {

/** Holds the read byte and meta-information about reading */
struct UniReadRes{
	int readBytes;
	wchar_t readChar;
};

/**
 * Can be used to provide your generic input handler
 * and serve as base-class for sample implementations.
 */
public class InHandler {
public:
	// The top bit is set as one, others zero!
	static const uint8_t topBit = 0x80;
	/** Implement this for your liking */
	virtual wchar_t readUnicodeChar() = const 0;

	UniReadRes readFromUtf8Buf(uint8_t* bytes) {
		// Copy pointer for us
		uint8_t *head = bytes;

		// First check top bit
		uint8_t bit = topBit;
		// if comes true when non-zero!
		if(topBit & *head){
			// ASCII 0..127 here
			// 1 byte is returned normally
			return UniReadRes{1, *head};
		} else {
			// See: man utf8
			// TODO: count ones
			// TODO: return val
			// TODO: exclude non-simple representations
		}
		
		return UniReadRes{0, 0};
	}
};

}
#endif
