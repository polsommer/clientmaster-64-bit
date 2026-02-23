// TwofishEncryptor.h
// copyright 2001 Verant Interactive
// Author: Justin Randall

#ifndef	_INCLUDED_TwofishEncryptor_H
#define	_INCLUDED_TwofishEncryptor_H

//-----------------------------------------------------------------------

#include <vector>

#include "TwofishCrypt.h"

namespace CryptoPP
{
	class TwofishEncryption;
}

namespace Crypto {

//-----------------------------------------------------------------------

class TwofishEncryptor : public TwofishCrypt
{
public:
	TwofishEncryptor();
	TwofishEncryptor(const unsigned char * const keyData, const unsigned int keyLength);
	~TwofishEncryptor();

	void setKey(const unsigned char * const keyData, const unsigned int keyLength);
	static void getLastKey(std::vector<unsigned char> & keyData);
	static unsigned int getLastKeyLength();
//	const unsigned int process(const unsigned char * const inputBuffer, unsigned char * outputBuffer, const unsigned int size);

private:
	TwofishEncryptor & operator = (const TwofishEncryptor & rhs);
	TwofishEncryptor(const TwofishEncryptor & source);
	static std::vector<unsigned char> s_lastKey;
};

//-----------------------------------------------------------------------

}//namespace Crypto

#endif	// _INCLUDED_TwofishEncryptor_H
