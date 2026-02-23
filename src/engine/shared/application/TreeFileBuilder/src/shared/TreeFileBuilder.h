// ======================================================================
//
// TreeFileBuilder.h
// ala diaz
//
// copyright 1999 Bootprint Entertainment
//
// ======================================================================

#ifndef INCLUDED_TreeFileBuilder_H
#define INCLUDED_TreeFileBuilder_H

// ======================================================================

#include "sharedFile/TreeFile.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/Md5.h"
#include <cstdio>
#include <vector>
#include <string>

// ======================================================================

class Compressor;

// ======================================================================

class TreeFileBuilder
{
private:

	TreeFileBuilder(void);
	TreeFileBuilder(const TreeFileBuilder&);
	TreeFileBuilder &operator =(const TreeFileBuilder&);

private:

        enum TransformMode
        {
                TM_encrypt,
                TM_decrypt
        };

        struct FileEntry
	{
	private:

		FileEntry(void);
		FileEntry(const FileEntry &);
		FileEntry &operator =(const FileEntry &);

	public:

		char           *diskFileEntry;
		CrcLowerString treeFileEntry;
		int            offset;
		int            length;
		int            compressor;
		int            compressedLength;
		Md5::Value     md5;
		bool           deleted;
		bool           uncompressed;
		FileEntry(const char *newName, const char *newlyChangedName);
		~FileEntry(void);
	};

private:

	char   *treeFileName;
	FILE   *treeFileHandle;
	int    numberOfFiles;
	int    totalFileSize;
	int    totalSmallestSize;
	int    sizeOfTOC;
	int    tocCompressorID;
	int    blockCompressorID;
	int    duplicateCount;
	int    sizeOfNameBlock;
	int    uncompSizeOfNameBlock;
	std::vector<FileEntry*> responseFileOrder;
        std::vector<FileEntry*> tocOrder;
        std::vector<const char *> fileNameBlock;
        bool   encryptContent;
        Md5::Value encryptionKey;
        uint32 encryptionOffset;

private:

        void write(const void *data, int length);
        void writeUnencrypted(const void *data, int length);
        void writeTableOfContents();
        void writeFileNameBlock();
        void writeMd5Block();
        void writeFile(FileEntry *fileEntry);
        void compressAndWrite(const byte * uncompressed, int &sizeOfData, const int uncompSize, int &compressor, const bool isFileData, const bool disableCompression, Md5::Value *md5sum);
        static bool transformTreeFile(const char *sourceFileName, const char *destinationFileName, Md5::Value const &key, TransformMode mode);

public:

	TreeFileBuilder(const char *fileName);
	~TreeFileBuilder(void);
	void createFile(void);
        void addFile(const char *diskFileNameEntry, const char *treeFileNameEntry, bool changedFileName, bool uncompressedFile);
        void addResponseFile(const char *responseFileEntry);
        void write(void);
        static bool LessFileEntryCrcNameCompare(const FileEntry* a, const FileEntry* b);
        void enableEncryption(const char *passphrase);
        void disableEncryption();
        bool isEncryptionEnabled() const;
        static bool encryptTreeFile(const char *sourceFileName, const char *destinationFileName, const char *passphrase);
        static bool decryptTreeFile(const char *sourceFileName, const char *destinationFileName, const char *passphrase);
};

// ======================================================================

#endif
