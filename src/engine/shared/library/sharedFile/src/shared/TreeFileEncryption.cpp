// ======================================================================
//
// TreeFileEncryption.cpp
//
// Implements shared helpers for encrypting and decrypting the payload of
// tree files.  The helpers intentionally keep the algorithm simple so the
// payload can be accessed in a streaming fashion without requiring the
// entire tree file to be loaded into memory.
//
// ======================================================================

#include "sharedFile/FirstSharedFile.h"
#include "sharedFile/TreeFileEncryption.h"

#include "sharedFoundation/Misc.h"

// ======================================================================

namespace TreeFileEncryptionNamespace
{
	inline uint8 const *getKeyData(Md5::Value const &key)
	{
		return static_cast<uint8 const *>(key.getConstData());
	}
}

using namespace TreeFileEncryptionNamespace;

// ======================================================================

Md5::Value TreeFileEncryption::deriveKey(char const *passphrase)
{
	if (!passphrase)
		return Md5::calculate("", 0);

	return Md5::calculate(passphrase, static_cast<int>(istrlen(passphrase)));
}

// ----------------------------------------------------------------------

bool TreeFileEncryption::isPassphraseValid(char const *passphrase)
{
	if (!passphrase)
		return false;

	return passphrase[0] != '\0';
}

// ----------------------------------------------------------------------

void TreeFileEncryption::transformBuffer(void *buffer, int length, Md5::Value const &key, uint32 offset)
{
	if (!buffer || length <= 0)
		return;

	uint8 *const bytes = static_cast<uint8 *>(buffer);
	uint8 const *const keyBytes = getKeyData(key);
	static size_t const keyLength = Md5::Value::cms_dataSize;

	for (int i = 0; i < length; ++i)
	{
		const uint32 keyIndex = (offset + static_cast<uint32>(i)) % static_cast<uint32>(keyLength);
		bytes[i] = static_cast<uint8>(bytes[i] ^ keyBytes[keyIndex]);
	}
}

// ======================================================================
