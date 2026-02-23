// ======================================================================
//
// ConfigSharedFile.h
// Copyright 2002, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ConfigSharedFile_H
#define INCLUDED_ConfigSharedFile_H

// ======================================================================

class ConfigSharedFile
{
public:
	static void install();

	static bool        getEnableAsynchronousLoader();
	static int         getAsynchronousLoaderPriority();
	static int         getAsynchronousLoaderCallbacksPerFrame();
        static bool        getValidateIff();
	static bool        getAllowMissingTOC();
        static int         getNumberOfTreeFilePreloads();
        static char const * getTreeFilePreload(int index);
        static char const * getTreeFileEncryptionPassphrase();
        static bool        getAllowEmptyTreeFileEncryptionPassphrase();
};

// ======================================================================

#endif
