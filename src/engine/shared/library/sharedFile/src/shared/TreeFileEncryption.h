// ======================================================================
//
// TreeFileEncryption.h
//
// Provides helper utilities for encrypting and decrypting tree file
// payloads using a lightweight XOR stream derived from a passphrase.
//
// ======================================================================

#ifndef INCLUDED_TreeFileEncryption_H
#define INCLUDED_TreeFileEncryption_H

// ======================================================================

#include "sharedFoundation/Md5.h"

// ======================================================================

namespace TreeFileEncryption
{
Md5::Value deriveKey(char const *passphrase);
bool      isPassphraseValid(char const *passphrase);
void      transformBuffer(void *buffer, int length, Md5::Value const &key, uint32 offset);
}

// ======================================================================

#endif

