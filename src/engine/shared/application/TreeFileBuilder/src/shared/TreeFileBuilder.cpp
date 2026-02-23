#include "FirstTreeFileBuilder.h"
#include "TreeFileBuilder.h"
#include "sharedCompression/ZlibCompressor.h"
#include "sharedCompression/SetupSharedCompression.h"
#include "sharedCompression/Compressor.h"
#include "sharedCompression/Lz77.h"
#include "sharedFile/TreeFile_SearchNode.h"
#include "sharedFile/TreeFileEncryption.h"
#include "sharedFile/ConfigSharedFile.h"
#include "sharedFoundation/CommandLine.h"
#include "sharedFoundation/Crc.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedIoWin/SetupSharedIoWin.h"
#include "sharedFile/SetupSharedFile.h"

#if defined(__has_include)
#if __has_include(<zlib.h>)
#include <zlib.h>
#elif __has_include("../../../../../../external/3rd/library/zlib/include/zlib.h")
#include "../../../../../../external/3rd/library/zlib/include/zlib.h"
#else
#error "zlib.h not found"
#endif
#else
#include "../../../../../../external/3rd/library/zlib/include/zlib.h"
#endif

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
// ======================================================================

namespace
{
	bool stringsEqualIgnoreCase(char const *lhs, char const *rhs)
	{
		if (!lhs || !rhs)
			return false;
		for (; *lhs && *rhs; ++lhs, ++rhs)
		if (std::tolower(static_cast<unsigned char>(*lhs)) != std::tolower(static_cast<unsigned char>(*rhs)))
			return false;
		return *lhs == '\0' && *rhs == '\0';
	}

	enum TreeFileOutputType
	{
		TFOT_unknown,
		TFOT_tre,
		TFOT_tres,
		TFOT_tresx
	};

	TreeFileOutputType getOutputType(char const *fileName)
	{
		if (!fileName)
			return TFOT_unknown;
		char const *extension = std::strrchr(fileName, '.');
		if (!extension || *(extension + 1) == '\0')
			return TFOT_unknown;
		if (stringsEqualIgnoreCase(extension + 1, "tre"))
			return TFOT_tre;
		if (stringsEqualIgnoreCase(extension + 1, "tres"))
			return TFOT_tres;
		if (stringsEqualIgnoreCase(extension + 1, "tresx"))
			return TFOT_tresx;
		return TFOT_unknown;
	}

	FILE *safeFileOpen(char const *fileName, char const *mode)
	{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
		FILE *file = nullptr;
		errno_t const result = ::fopen_s(&file, fileName, mode);
		if (result != 0)
			return nullptr;
		return file;
#else
		return std::fopen(fileName, mode);
#endif
	}

	size_t safeFileWrite(void const *buffer, size_t byteCount, FILE *file)
	{
		if (byteCount == 0)
			return 0;
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && defined(__STDC_LIB_EXT1__)
		return ::fwrite_s(buffer, byteCount, 1, byteCount, file);
#else
#if defined(_MSC_VER)
#pragma warning(suppress : 4996)
#endif
		return std::fwrite(buffer, 1, byteCount, file);
#endif
	}

	size_t safeFileRead(void *buffer, size_t byteCount, FILE *file)
	{
		if (byteCount == 0)
			return 0;
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && defined(__STDC_LIB_EXT1__)
		return ::fread_s(buffer, byteCount, 1, byteCount, file);
#else
#if defined(_MSC_VER)
#pragma warning(suppress : 4996)
#endif
		return std::fread(buffer, 1, byteCount, file);
#endif
	}

	int safeFileSeek(FILE *file, long offset, int origin)
	{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
		return ::_fseeki64(file, static_cast<__int64>(offset), origin);
#else
#if defined(_MSC_VER)
#pragma warning(suppress : 4996)
#endif
		return std::fseek(file, offset, origin);
#endif
	}

	long safeFileTell(FILE *file)
	{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
		__int64 const position = ::_ftelli64(file);
		if (position < static_cast<__int64>(LONG_MIN) || position > static_cast<__int64>(LONG_MAX))
			return -1L;
		return static_cast<long>(position);
#else
#if defined(_MSC_VER)
#pragma warning(suppress : 4996)
#endif
		return std::ftell(file);
#endif
	}

	int safeFileClose(FILE *file)
	{
#if defined(_MSC_VER)
#pragma warning(suppress : 4996)
#endif
		return std::fclose(file);
	}
}

const Tag TAG_TREE = TAG(T, R, E, E);
const Tag TAG_TRES = TAG(T, R, E, S);
const Tag TAG_TRESX = TAG(T, R, S, X);
static int errors = 0;
// ======================================================================

bool TreeFileBuilder::transformTreeFile(const char *sourceFileName, const char *destinationFileName, Md5::Value const &key, TransformMode mode)
{
        if (!sourceFileName || !destinationFileName)
        {
                fprintf(stderr, "Invalid source or destination specified.\n");
                return false;
        }

        FILE *source = safeFileOpen(sourceFileName, "rb");
        if (!source)
        {
                fprintf(stderr, "Unable to open source tree file %s: %s\n", sourceFileName, std::strerror(errno));
                return false;
        }

        FILE *destination = safeFileOpen(destinationFileName, "wb");
        if (!destination)
        {
                fprintf(stderr, "Unable to open destination tree file %s: %s\n", destinationFileName, std::strerror(errno));
                safeFileClose(source);
                return false;
        }

        TreeFile::SearchTree::Header header;
        size_t const headerBytes = safeFileRead(&header, sizeof(header), source);
        if (headerBytes != sizeof(header))
        {
                fprintf(stderr, "Unable to read tree file header from %s\n", sourceFileName);
                safeFileClose(source);
                safeFileClose(destination);
                return false;
        }

        if (mode == TM_encrypt)
        {
                if (header.token != TAG_TREE)
                {
                        fprintf(stderr, "Source file is not an unencrypted tree file: %s\n", sourceFileName);
                        safeFileClose(source);
                        safeFileClose(destination);
                        return false;
                }
                header.token = TAG_TRES;
        }
        else
        {
                if (header.token != TAG_TRES)
                {
                        fprintf(stderr, "Source file is not an encrypted tree file: %s\n", sourceFileName);
                        safeFileClose(source);
                        safeFileClose(destination);
                        return false;
                }
                header.token = TAG_TREE;
        }

        if (safeFileWrite(&header, sizeof(header), destination) != sizeof(header))
        {
                fprintf(stderr, "Unable to write tree file header to %s\n", destinationFileName);
                safeFileClose(source);
                safeFileClose(destination);
                return false;
        }

        std::vector<unsigned char> buffer(64 * 1024);
        uint32 offset = 0;

        for (;;)
        {
                size_t const bytesRead = safeFileRead(&buffer[0], buffer.size(), source);
                if (bytesRead == 0)
                {
                        if (std::ferror(source))
                        {
                                fprintf(stderr, "Error reading from %s: %s\n", sourceFileName, std::strerror(errno));
                                safeFileClose(source);
                                safeFileClose(destination);
                                return false;
                        }
                        break;
                }

                TreeFileEncryption::transformBuffer(&buffer[0], static_cast<int>(bytesRead), key, offset);

                if (safeFileWrite(&buffer[0], bytesRead, destination) != bytesRead)
                {
                        fprintf(stderr, "Error writing transformed data to %s: %s\n", destinationFileName, std::strerror(errno));
                        safeFileClose(source);
                        safeFileClose(destination);
                        return false;
                }

                offset += static_cast<uint32>(bytesRead);
        }

        safeFileClose(source);
        safeFileClose(destination);

        return true;
}

// ======================================================================

static void run(void);
static bool runInteractiveWizard(void);
static bool runInteractiveBuildWorkflow(void);
static bool runInteractiveDecryptWorkflow(void);
static bool promptForLine(const char *prompt, bool allowEmpty, std::string &value);
static bool promptForExistingFile(const char *prompt, std::string &value);
static bool promptForYesNo(const char *prompt, bool defaultValue);
static bool readLine(std::string &value);
static void trimWhitespace(std::string &value);
static void usage(void);
static void printOptionReference(void);
static void logDiagnostics(const char *format, ...);
static void logDiagnosticsLine(const char *format, va_list args);
static void printConfigurationSummary(const char *outputTreeFileName, const char *responseFile, bool encryptionRequested, bool passphraseProvided);
static void printDecryptionSummary(const char *source, const char *destination, bool passphraseProvided);
static void waitForUserExitIfRequested(void);

// ======================================================================
// command line stuff

static const char * const LNAME_HELP                             = "help";
static const char * const LNAME_RSP_FILE                         = "responseFile";
static const char * const LNAME_NO_TOC_COMPRESSION       = "noTOCCompression";
static const char * const LNAME_NO_FILE_COMPRESSION  = "noFileCompression";
static const char * const LNAME_NO_CREATE                    = "noCreate";
static const char * const LNAME_QUIET                = "quiet";
static const char * const LNAME_ENCRYPT              = "encrypt";
static const char * const LNAME_NO_ENCRYPT           = "noEncrypt";
static const char * const LNAME_PASSPHRASE           = "passphrase";
static const char * const LNAME_DECRYPT              = "decrypt";
static const char * const LNAME_DECRYPT_OUTPUT       = "decryptOutput";
static const char * const LNAME_DEBUG                = "debug";
static const char * const LNAME_LIST_OPTIONS         = "listOptions";
static const char         SNAME_HELP                            = 'h';
static const char         SNAME_RSP_FILE                        = 'r';
static const char         SNAME_NO_TOC_COMPRESSION      = 't';
static const char         SNAME_NO_FILE_COMPRESSION = 'f';
static const char         SNAME_NO_CREATE                       = 'c';
static const char         SNAME_QUIET                   = 'q';
static const char         SNAME_ENCRYPT                 = 'e';
static const char         SNAME_NO_ENCRYPT              = 'n';
static const char         SNAME_PASSPHRASE              = 'p';
static const char         SNAME_DECRYPT                 = 'd';
static const char         SNAME_DECRYPT_OUTPUT  = 'o';
static const char         SNAME_DEBUG                   = 'g';
static const char         SNAME_LIST_OPTIONS            = 'l';

static CommandLine::OptionSpec optionSpecArray[] =
{
        OP_BEGIN_SWITCH(OP_NODE_REQUIRED),

                // help
                OP_SINGLE_SWITCH_NODE(SNAME_HELP, LNAME_HELP, OP_ARG_NONE, OP_MULTIPLE_DENIED),

                // real options
                OP_BEGIN_SWITCH_NODE(OP_MULTIPLE_DENIED),
                        OP_BEGIN_LIST(),

                                // rsp file (optional when decrypting)
                                OP_SINGLE_LIST_NODE(SNAME_RSP_FILE, LNAME_RSP_FILE, OP_ARG_REQUIRED, OP_MULTIPLE_DENIED,  OP_NODE_OPTIONAL),

                                // if specified, don't use compression on file entries
                                OP_SINGLE_LIST_NODE(SNAME_NO_TOC_COMPRESSION, LNAME_NO_TOC_COMPRESSION, OP_ARG_NONE, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),

                                // if specified, don't use compression on file entries and TOC
                                OP_SINGLE_LIST_NODE(SNAME_NO_FILE_COMPRESSION, LNAME_NO_FILE_COMPRESSION, OP_ARG_NONE, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),

                                // if specified, don't create the treefile
                                OP_SINGLE_LIST_NODE(SNAME_NO_CREATE, LNAME_NO_CREATE, OP_ARG_NONE, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),

                                OP_SINGLE_LIST_NODE(SNAME_QUIET, LNAME_QUIET, OP_ARG_NONE, OP_MULTIPLE_ALLOWED, OP_NODE_OPTIONAL),
                                OP_SINGLE_LIST_NODE(SNAME_ENCRYPT, LNAME_ENCRYPT, OP_ARG_NONE, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
                                OP_SINGLE_LIST_NODE(SNAME_NO_ENCRYPT, LNAME_NO_ENCRYPT, OP_ARG_NONE, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
                                OP_SINGLE_LIST_NODE(SNAME_PASSPHRASE, LNAME_PASSPHRASE, OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
                                OP_SINGLE_LIST_NODE(SNAME_DECRYPT, LNAME_DECRYPT, OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
                                OP_SINGLE_LIST_NODE(SNAME_DECRYPT_OUTPUT, LNAME_DECRYPT_OUTPUT, OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
                                OP_SINGLE_LIST_NODE(SNAME_DEBUG, LNAME_DEBUG, OP_ARG_NONE, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),
                                OP_SINGLE_LIST_NODE(SNAME_LIST_OPTIONS, LNAME_LIST_OPTIONS, OP_ARG_NONE, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),

                                // get the output tree file name
                                OP_SINGLE_LIST_NODE(OP_SNAME_UNTAGGED, OP_LNAME_UNTAGGED, OP_ARG_REQUIRED, OP_MULTIPLE_DENIED, OP_NODE_OPTIONAL),

                        OP_END_LIST(),
                OP_END_SWITCH_NODE(),

        OP_END_SWITCH()
};

static const int optionSpecCount = sizeof(optionSpecArray) / sizeof(optionSpecArray[0]);

static bool      disableTOCCompression;
static bool      disableFileCompression;
static bool      disableCreation;
static int       quiet;
static bool      diagnosticsEnabled;
static bool      listOptionsRequested;
static bool      waitForUserExit;
static bool      interactiveMode;

static std::vector<std::string> warnings;

// ======================================================================

int main(int argc, char **argv)
{
        interactiveMode = (argc <= 1);
        waitForUserExit = interactiveMode;

        //-- ensure stdout/stderr flush immediately when the process is
        //   launched without an attached console (for example when the
        //   Python wrapper captures output through pipes).
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);

        //-- thread
        SetupSharedThread::install();

        //-- debug
        SetupSharedDebug::install(4096);

	{
		SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_console);
		data.argc  = argc;
		data.argv  = argv;
		SetupSharedFoundation::install(data);
	}

	SetupSharedCompression::install();

	//-- file
	SetupSharedFile::install(false);

	//-- iowin
	SetupSharedIoWin::install();

	SetupSharedFoundation::callbackWithExceptionHandling(run);
	SetupSharedFoundation::remove();
	SetupSharedThread::remove();

	waitForUserExitIfRequested();

	return -errors;
}

// ----------------------------------------------------------------------

class TreeFileBuilderHelper
{
public:

	static const Tag getToken()
	{
		return TAG(T,R,E,E);
	}

	static const Tag getVersion()
	{
		return TAG_0005;
	}
};

// ----------------------------------------------------------------------

static void run(void)
{
        if (interactiveMode)
        {
                runInteractiveWizard();
                return;
        }

        // handle options
        const CommandLine::MatchCode mc = CommandLine::parseOptions(optionSpecArray, optionSpecCount);

        if (mc != CommandLine::MC_MATCH)
	{
		printf("Invalid command line specified.  Printing usage...\n");
		usage();
		if (waitForUserExit)
			printf("\nTreeFileBuilder is a command-line tool. Provide the required arguments or run with --help for more information.\n");
		++errors;
		return;
	}

        diagnosticsEnabled = (CommandLine::getOccurrenceCount(SNAME_DEBUG) != 0);
        listOptionsRequested = (CommandLine::getOccurrenceCount(SNAME_LIST_OPTIONS) != 0);

        if (listOptionsRequested)
        {
                printOptionReference();

                if (!diagnosticsEnabled)
                        return;
        }

        if (CommandLine::getOccurrenceCount(SNAME_HELP))
        {
                usage();
                return;
        }

        if (CommandLine::getOccurrenceCount(SNAME_NO_TOC_COMPRESSION))
                disableTOCCompression = true;

        if (CommandLine::getOccurrenceCount(SNAME_NO_FILE_COMPRESSION))
                disableFileCompression = true;

        if (CommandLine::getOccurrenceCount(SNAME_NO_CREATE))
                disableCreation = true;

        quiet = CommandLine::getOccurrenceCount(SNAME_QUIET);

        const char *const decryptSource = CommandLine::getOptionString(SNAME_DECRYPT);
        const bool decryptMode = (decryptSource != NULL);
        const char *const decryptOutputOption = CommandLine::getOptionString(SNAME_DECRYPT_OUTPUT);

        char const * const configuredPassphrase = ConfigSharedFile::getTreeFileEncryptionPassphrase();
        std::string passphrase = configuredPassphrase ? configuredPassphrase : "";
        if (const char *overridePassphrase = CommandLine::getOptionString(SNAME_PASSPHRASE))
                passphrase = overridePassphrase;

        bool passphraseProvided = !passphrase.empty();

        if (decryptMode)
        {
                const char *destinationFile = decryptOutputOption;
                if (!destinationFile)
                        destinationFile = CommandLine::getUntaggedString(0);

                if (!decryptSource || !destinationFile)
                {
                        fprintf(stderr, "Decrypt mode requires --%s=<source> and an output file (either positional or --%s).\n", LNAME_DECRYPT, LNAME_DECRYPT_OUTPUT);
                        ++errors;
                        return;
                }

                printDecryptionSummary(decryptSource, destinationFile, passphraseProvided);

                if (!TreeFileBuilder::decryptTreeFile(decryptSource, destinationFile, passphrase.c_str()))
                {
                        logDiagnostics("Decryption failed for '%s'.", decryptSource);
                        ++errors;
                }
                else
                {
                        printf("Decrypted '%s' to '%s'.\n", decryptSource, destinationFile);
                        logDiagnostics("Decryption succeeded.");
                }

                return;
        }

        const char *const outputTreeFileName = CommandLine::getUntaggedString(0);

        if (!outputTreeFileName)
        {
                fprintf(stderr, "An output tree file name must be specified.\n");
                ++errors;
                return;
        }

        const char *const responseFile = CommandLine::getOptionString(SNAME_RSP_FILE);

        if (!responseFile)
        {
                fprintf(stderr, "A response file must be specified using -%c or --%s.\n", SNAME_RSP_FILE, LNAME_RSP_FILE);
                ++errors;
                return;
        }

        TreeFileOutputType const outputType = getOutputType(outputTreeFileName);
        bool encryptionRequested = (outputType == TFOT_tres || outputType == TFOT_tresx);

        if (CommandLine::getOccurrenceCount(SNAME_ENCRYPT))
                encryptionRequested = true;

        if (CommandLine::getOccurrenceCount(SNAME_NO_ENCRYPT))
                encryptionRequested = false;

        printConfigurationSummary(outputTreeFileName, responseFile, encryptionRequested, passphraseProvided);

        // a valid set of command line options has been specified
        TreeFileBuilder t(outputTreeFileName);

        if (encryptionRequested)
                t.enableEncryption(passphrase.c_str());
        else
                t.disableEncryption();

        if (errors)
                return;

        t.addResponseFile(responseFile);

        if (errors)
                return;

        if (disableCreation)
        {
                printf("Scan complete. No errors.\n");
                logDiagnostics("Scan completed without creating '%s'.", outputTreeFileName);
                return;
        }

        t.createFile();

        if (errors)
                return;

        logDiagnostics("Population pass complete for '%s'.", outputTreeFileName);

        t.write();

        if (warnings.size ())
        {
                printf ("Warnings:\n");

                uint i;
                for (i = 0; i < warnings.size (); ++i)
                        printf ("  %s", warnings [i].c_str ());

                logDiagnostics("%u warning(s) emitted during the build.", static_cast<unsigned int>(warnings.size()));
        }
        else
        {
                logDiagnostics("Tree build finished without warnings.");
        }

        logDiagnostics("Tree file '%s' written successfully.", outputTreeFileName);
}

// ----------------------------------------------------------------------

static void usage(void)
{
        #include "TreeFileBuilder.dox"
}

// ----------------------------------------------------------------------

static void printOptionReference(void)
{
        printf("TreeFileBuilder command line options:\n");
        printf("  -%c, --%s <file>       Response file describing the assets to include.\n", SNAME_RSP_FILE, LNAME_RSP_FILE);
        printf("  -%c, --%s              Disable TOC compression (data compression still allowed).\n", SNAME_NO_TOC_COMPRESSION, LNAME_NO_TOC_COMPRESSION);
        printf("  -%c, --%s              Disable all compression (TOC and data).\n", SNAME_NO_FILE_COMPRESSION, LNAME_NO_FILE_COMPRESSION);
        printf("  -%c, --%s              Scan only; do not create an output file.\n", SNAME_NO_CREATE, LNAME_NO_CREATE);
        printf("  -%c, --%s              Reduce console chatter (may be supplied multiple times).\n", SNAME_QUIET, LNAME_QUIET);
        printf("  -%c, --%s              Force encryption using the configured or provided passphrase.\n", SNAME_ENCRYPT, LNAME_ENCRYPT);
        printf("  -%c, --%s              Force encryption off even if the output extension suggests it.\n", SNAME_NO_ENCRYPT, LNAME_NO_ENCRYPT);
        printf("  -%c, --%s <text>       Override the encryption passphrase.\n", SNAME_PASSPHRASE, LNAME_PASSPHRASE);
        printf("  -%c, --%s <source>     Decrypt the specified tree file.\n", SNAME_DECRYPT, LNAME_DECRYPT);
        printf("  -%c, --%s <file>   Destination when decrypting (defaults to positional argument).\n", SNAME_DECRYPT_OUTPUT, LNAME_DECRYPT_OUTPUT);
        printf("  -%c, --%s              Emit detailed diagnostics about the resolved configuration.\n", SNAME_DEBUG, LNAME_DEBUG);
        printf("  -%c, --%s              Print this option reference. Combine with --%s to continue execution.\n", SNAME_LIST_OPTIONS, LNAME_LIST_OPTIONS, LNAME_DEBUG);
        printf("  -%c, --%s              Display help/usage information.\n", SNAME_HELP, LNAME_HELP);
        printf("      <output>            Positional argument naming the output tree file.\n");
        printf("\n");
}

// ----------------------------------------------------------------------

static void logDiagnostics(const char *format, ...)
{
        if (!diagnosticsEnabled)
                return;

        va_list args;
        va_start(args, format);
        logDiagnosticsLine(format, args);
        va_end(args);
}

// ----------------------------------------------------------------------

static void logDiagnosticsLine(const char *format, va_list args)
{
        if (!diagnosticsEnabled)
                return;

        std::vfprintf(stdout, format, args);

        size_t const length = std::strlen(format);
        if (length == 0 || format[length - 1] != '\n')
                std::fputc('\n', stdout);

        std::fflush(stdout);
}

// ----------------------------------------------------------------------

static void printConfigurationSummary(const char *outputTreeFileName, const char *responseFile, bool encryptionRequested, bool passphraseProvided)
{
        if (!diagnosticsEnabled)
                return;

        logDiagnostics("Execution plan:");
        logDiagnostics("  Output tree file : %s", outputTreeFileName ? outputTreeFileName : "<not provided>");
        logDiagnostics("  Response file    : %s", responseFile ? responseFile : "<not provided>");
        const char *encryptionStatus = "disabled";
        if (encryptionRequested)
                encryptionStatus = passphraseProvided ? "enabled" : "enabled (empty passphrase)";
        logDiagnostics("  Encryption       : %s", encryptionStatus);
        logDiagnostics("  Quiet level      : %d", quiet);
        logDiagnostics("  TOC compression  : %s", disableTOCCompression ? "disabled" : "enabled");
        logDiagnostics("  File compression : %s", disableFileCompression ? "disabled" : "enabled");
        logDiagnostics("  Creation         : %s", disableCreation ? "disabled (scan only)" : "enabled");
        logDiagnostics("  List options     : %s", listOptionsRequested ? "requested" : "not requested");
}

// ----------------------------------------------------------------------

static void printDecryptionSummary(const char *source, const char *destination, bool passphraseProvided)
{
        if (!diagnosticsEnabled)
                return;

        logDiagnostics("Decryption plan:");
        logDiagnostics("  Source      : %s", source ? source : "<not provided>");
        logDiagnostics("  Destination : %s", destination ? destination : "<not provided>");
        logDiagnostics("  Passphrase  : %s", passphraseProvided ? "provided" : "empty");
}

// ----------------------------------------------------------------------

static void trimWhitespace(std::string &value)
{
        size_t start = 0;
        size_t end = value.length();

        while (start < end && std::isspace(static_cast<unsigned char>(value[start])))
                ++start;

        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
                --end;

        if (start == 0 && end == value.length())
                return;

        value.assign(value, start, end - start);
}

// ----------------------------------------------------------------------

static bool readLine(std::string &value)
{
        value.clear();

        char buffer[1024];
        bool anyInput = false;

        for (;;)
        {
                if (!std::fgets(buffer, sizeof(buffer), stdin))
                {
                        if (!anyInput)
                                return false;
                        break;
                }

                anyInput = true;
                value.append(buffer);

                size_t const currentLength = std::strlen(buffer);
                if (currentLength == 0)
                        break;

                if (buffer[currentLength - 1] == '\n')
                        break;
        }

        while (!value.empty())
        {
                char const lastChar = value[value.size() - 1];
                if (lastChar != '\n' && lastChar != '\r')
                        break;

                value.erase(value.end() - 1);
        }

        return true;
}

// ----------------------------------------------------------------------

static bool promptForLine(const char *prompt, bool allowEmpty, std::string &value)
{
        if (!prompt)
                prompt = "";

        for (;;)
        {
                printf("%s", prompt);
                fflush(stdout);

                if (!readLine(value))
                        return false;

                trimWhitespace(value);

                if (!value.empty() || allowEmpty)
                        return true;

                printf("A value is required. Please try again.\n");
        }
}

// ----------------------------------------------------------------------

static bool promptForExistingFile(const char *prompt, std::string &value)
{
        for (;;)
        {
                if (!promptForLine(prompt, false, value))
                        return false;

                FILE *file = safeFileOpen(value.c_str(), "rb");
                if (file)
                {
                        safeFileClose(file);
                        return true;
                }

                printf("Unable to open '%s': %s\n", value.c_str(), std::strerror(errno));
        }
}

// ----------------------------------------------------------------------

static bool promptForYesNo(const char *prompt, bool defaultValue)
{
        if (!prompt)
                prompt = "";

        std::string response;

        for (;;)
        {
                printf("%s", prompt);
                fflush(stdout);

                if (!readLine(response))
                        return defaultValue;

                trimWhitespace(response);

                if (response.empty())
                        return defaultValue;

                char const choice = static_cast<char>(std::tolower(static_cast<unsigned char>(response[0])));

                if (choice == 'y')
                        return true;

                if (choice == 'n')
                        return false;

                printf("Please enter 'y' or 'n'.\n");
        }
}

// ----------------------------------------------------------------------

static bool runInteractiveBuildWorkflow(void)
{
        warnings.clear();
        errors = 0;
        disableTOCCompression = false;
        disableFileCompression = false;
        disableCreation = false;
        diagnosticsEnabled = false;
        listOptionsRequested = false;
        quiet = 0;

        printf("\nTreeFileBuilder interactive build\n");
        printf("---------------------------------\n");

        std::string responseFile;
        if (!promptForExistingFile("Response file (.rsp): ", responseFile))
                return false;

        std::string defaultOutput = responseFile;
        size_t const extensionOffset = defaultOutput.find_last_of('.');
        if (extensionOffset != std::string::npos)
                defaultOutput.erase(extensionOffset);
        defaultOutput += ".tre";

        std::string outputPrompt = std::string("Output tree file (.tre/.tres/.tresx) [") + defaultOutput + "]: ";
        std::string outputTreeFile;
        if (!promptForLine(outputPrompt.c_str(), true, outputTreeFile))
                return false;

        if (outputTreeFile.empty())
                outputTreeFile = defaultOutput;

        TreeFileOutputType const outputType = getOutputType(outputTreeFile.c_str());
        bool const encryptionDefault = (outputType == TFOT_tres || outputType == TFOT_tresx);
        std::string encryptionPrompt = encryptionDefault ? "Encrypt output? [Y/n]: " : "Encrypt output? [y/N]: ";
        bool const encryptionRequested = promptForYesNo(encryptionPrompt.c_str(), encryptionDefault);

        char const *configuredPassphrase = ConfigSharedFile::getTreeFileEncryptionPassphrase();
        std::string passphrase = configuredPassphrase ? configuredPassphrase : "";
        bool passphraseProvided = !passphrase.empty();

        if (encryptionRequested && !passphraseProvided)
        {
                std::string passphraseInput;
                if (!promptForLine("Enter passphrase (optional): ", true, passphraseInput))
                        return false;
                if (!passphraseInput.empty())
                {
                        passphrase = passphraseInput;
                        passphraseProvided = true;
                }
        }

        if (!encryptionRequested)
        {
                passphrase.clear();
                passphraseProvided = false;
        }

        printf("\nConfiguration summary:\n");
        printf("  Response file : %s\n", responseFile.c_str());
        printf("  Output file   : %s\n", outputTreeFile.c_str());
        const char *encryptionStatus = "disabled";
        if (encryptionRequested)
                encryptionStatus = passphraseProvided ? "enabled" : "enabled (empty passphrase)";
        printf("  Encryption    : %s\n", encryptionStatus);

        TreeFileBuilder builder(outputTreeFile.c_str());

        if (encryptionRequested)
                builder.enableEncryption(passphrase.c_str());
        else
                builder.disableEncryption();

        if (errors)
                return false;

        builder.addResponseFile(responseFile.c_str());

        if (errors)
                return false;

        builder.createFile();

        if (errors)
                return false;

        builder.write();

        if (errors)
                return false;

        if (!warnings.empty())
        {
                printf("\nWarnings:\n");
                for (std::vector<std::string>::const_iterator iter = warnings.begin(); iter != warnings.end(); ++iter)
                        printf("  %s", (*iter).c_str());
        }

        printf("\nTree file '%s' created successfully.\n", outputTreeFile.c_str());
        return true;
}

// ----------------------------------------------------------------------

static bool runInteractiveDecryptWorkflow(void)
{
        warnings.clear();
        errors = 0;
        diagnosticsEnabled = false;
        quiet = 0;

        printf("\nTreeFileBuilder interactive decrypt\n");
        printf("-----------------------------------\n");

        std::string sourceFile;
        if (!promptForExistingFile("Encrypted tree file (.tres/.tresx): ", sourceFile))
                return false;

        std::string defaultDestination = sourceFile;
        size_t const extensionOffset = defaultDestination.find_last_of('.');
        if (extensionOffset != std::string::npos)
                defaultDestination.erase(extensionOffset);
        defaultDestination += ".tre";

        std::string destinationPrompt = std::string("Destination file [") + defaultDestination + "]: ";
        std::string destinationFile;
        if (!promptForLine(destinationPrompt.c_str(), true, destinationFile))
                return false;

        if (destinationFile.empty())
                destinationFile = defaultDestination;

        char const *configuredPassphrase = ConfigSharedFile::getTreeFileEncryptionPassphrase();
        std::string passphrase = configuredPassphrase ? configuredPassphrase : "";
        bool passphraseProvided = !passphrase.empty();

        if (!passphraseProvided)
        {
                std::string passphraseInput;
                if (!promptForLine("Enter passphrase (leave blank if none): ", true, passphraseInput))
                        return false;
                if (!passphraseInput.empty())
                        passphrase = passphraseInput;
        }

        if (!TreeFileBuilder::decryptTreeFile(sourceFile.c_str(), destinationFile.c_str(), passphrase.c_str()))
        {
                printf("Decryption failed. Verify the passphrase and file paths and try again.\n");
                ++errors;
                return false;
        }

        printf("Decrypted '%s' to '%s'.\n", sourceFile.c_str(), destinationFile.c_str());
        return true;
}

// ----------------------------------------------------------------------

static bool runInteractiveWizard(void)
{
        printf("TreeFileBuilder interactive mode\n");
        printf("===============================\n\n");
        printf("This mode guides you through building or decrypting a tree file without requiring command line arguments.\n");
        printf("Press Ctrl+C at any time to cancel.\n");

        for (;;)
        {
                std::string selection;
                if (!promptForLine("\nChoose an action: [B]uild, [D]ecrypt, [Q]uit: ", true, selection))
                        return false;

                if (selection.empty())
                        selection = "b";

                char const choice = static_cast<char>(std::tolower(static_cast<unsigned char>(selection[0])));

                if (choice == 'b')
                {
                        if (runInteractiveBuildWorkflow())
                                return true;

                        printf("\nBuild did not complete. You can adjust your inputs and try again.\n");
                        continue;
                }

                if (choice == 'd')
                {
                        if (runInteractiveDecryptWorkflow())
                                return true;

                        printf("\nDecryption did not complete. You can adjust your inputs and try again.\n");
                        continue;
                }

                if (choice == 'q')
                {
                        printf("No action selected. Exiting.\n");
                        return true;
                }

                printf("Unrecognized selection '%s'. Please enter B, D, or Q.\n", selection.c_str());
        }
}

// ----------------------------------------------------------------------

static void waitForUserExitIfRequested(void)
{
        if (!waitForUserExit)
                return;

#if defined(_WIN32)
        int const stdinDescriptor = _fileno(stdin);
        bool const stdinIsInteractive = (stdinDescriptor >= 0) && (_isatty(stdinDescriptor) != 0);
#else
        int const stdinDescriptor = fileno(stdin);
        bool const stdinIsInteractive = (stdinDescriptor >= 0) && (isatty(stdinDescriptor) != 0);
#endif

        if (!stdinIsInteractive)
                return;

        printf("\nPress Enter to exit...");
        fflush(stdout);

        int ch;
        do
        {
                ch = std::getchar();
        }
        while ((ch != '\n') && (ch != EOF));
}

// ======================================================================

TreeFileBuilder::FileEntry::FileEntry(const char *diskFileName, const char *treeFileName)
: diskFileEntry(DuplicateString(diskFileName)),
	treeFileEntry(treeFileName),
	offset(0),
	length(0),
	compressor(0),
	compressedLength(0),
	deleted(false),
	uncompressed(false)
{
}

// ----------------------------------------------------------------------

TreeFileBuilder::FileEntry::~FileEntry(void)
{
}

// ======================================================================

TreeFileBuilder::TreeFileBuilder(const char *newTreeFileName)
: treeFileName(DuplicateString(newTreeFileName)),
	treeFileHandle(NULL),
	numberOfFiles(0),
	totalFileSize(0),
	totalSmallestSize(0),
	sizeOfTOC(0),
	tocCompressorID(0),
	blockCompressorID(0),
	duplicateCount(0),
	uncompSizeOfNameBlock(0),
	encryptContent(false),
	encryptionKey(),
	encryptionOffset(0)
{
	DEBUG_FATAL(!treeFileName, ("treeFileName may not be NULL"));
}

// ----------------------------------------------------------------------

TreeFileBuilder::~TreeFileBuilder(void)
{
	if (treeFileHandle != NULL)
	{
		const int result = safeFileClose(treeFileHandle);
		FATAL(result != 0, ("error closing treefile '%s'", treeFileName));
		treeFileHandle = NULL;
	}

	if (errors)
	{
		if (std::remove(treeFileName) != 0)
			printf("Could not delete '%s'.\n", treeFileName);
	}

	delete [] treeFileName;

	std::vector<FileEntry*>::iterator iter = tocOrder.begin();

	// takes care of referenced memory in both responseFileOrder and tocOrder
	for (; iter != tocOrder.end(); ++iter)
	{
		delete [] (*iter)->diskFileEntry;
		delete *iter;
	}
}

// ----------------------------------------------------------------------

void TreeFileBuilder::createFile(void)
{
	treeFileHandle = safeFileOpen(treeFileName, "wb+");

	if (!treeFileHandle)
	{
		fprintf(stderr, "Error opening output TreeFile %s: %s\n", treeFileName, std::strerror(errno));
		++errors;
	}
}

// ----------------------------------------------------------------------

bool TreeFileBuilder::LessFileEntryCrcNameCompare(const FileEntry* a, const FileEntry* b)
{
	// comparing new file entry with iterator to determine insert index
	return (a->treeFileEntry < b->treeFileEntry);
}

// ----------------------------------------------------------------------

static bool isjunk(int ch)
{
	return isspace(ch) || ch == '\n' || ch == '\r';
}

// ----------------------------------------------------------------------

void TreeFileBuilder::addResponseFile(const char *responseFileName)
{
	char currentBuffer [10 * 1024];
	char nameBuffer[10 * 1024];

	printf("Processing response file %s\n", responseFileName);
	FILE *file = safeFileOpen(responseFileName, "rt");

	if (!file)
	{
		fprintf(stderr, "Unable to open response file %s\n", responseFileName);
		++errors;
		return;
	}

	for (;;)
	{
		nameBuffer[0] = '\0';
		currentBuffer[0] = '\0';

		// new name that may be found within current
		char *newName = nameBuffer;

		// get the whole line
		if (fgets(currentBuffer, sizeof(currentBuffer), file) == NULL)
			break;

		// ignore line if it doesn't have an @
		if (strstr(currentBuffer, "@") == 0)
			continue;

		// strip out "TF::open(x) " spit out in output window with the DFP_treeFileLog flag
		char *current = currentBuffer;
		char *start = strstr(current, "TF::open");
		bool validTFopen = false;
		if (start)
		{
			current = start + 12;

			// find ,
			char *end = strstr(current, ",");
			if (end)
			{
				validTFopen = true;
				*end = '\0';
			}
		}

		// lop off leading whitespace
		if (!validTFopen)
		{
			while (isjunk(*current))
				++current;

			//lop off trailing whitespace
			while (strlen(current) && isjunk(current[strlen(current)-1]))
				 current[strlen(current)-1] = '\0';
		}

		// will indicate if '@' name change switch is in the current file name
		bool switchEncountered = false;

		// will indicate if 'u' followed name change switch - designating the file should not be compressed
		bool uncompressedFile = false;

		// will update to the index of the first char in disk file name after new name is removed from the front
		int index=0;

		// examine every character in the file name
		for (int i=0; current[i] != '\0'; ++i)
		{
			if(current[i] == '@')
			{
				//insert all chars up to the space before the '@' symbol
				for(int k=0; k < i-1; ++k)
					newName[k] = current[k];

				//check to see if file should not be compressed
				if(current[i+1] == 'u')
					uncompressedFile = true;

				switchEncountered = true;

				//terminate string that is the new name (the name stored in the tree file)
				newName[i-1] = '\0';

				//update the index to the first char of the disk file name
				i += uncompressedFile ? 3 : 2;
			}

			if (switchEncountered)
				//update current without the new name on the front starting at index = 0
				current[index++] = current[i];
		}

		if (switchEncountered)
			//null terminate the reset file name with the new file name removed from front
			current[index] = '\0';
		else
			newName = current;

		addFile(current, newName, switchEncountered, uncompressedFile);

		if (errors)
		{
			safeFileClose(file);
			return;
		}

	}//end for

	printf("Added %d files with %d duplicate file(s)\n", numberOfFiles, duplicateCount);
	safeFileClose(file);
}

// ----------------------------------------------------------------------

void TreeFileBuilder::addFile(const char *diskFileNameEntry, const char *treeFileNameEntry, bool changingFileName, bool uncompressedFile)
{
	const char *src;
	char       *dest;
	char	     lowerTreeFileName[Os::MAX_PATH_LENGTH];
	const int  len = strlen(treeFileNameEntry);

	src = lowerTreeFileName;
	FATAL(len >= Os::MAX_PATH_LENGTH, ("File name too long: '%s'", treeFileNameEntry));

	// copy and lowercase the tree file name
	for (src = treeFileNameEntry, dest = lowerTreeFileName; *src; ++src, ++dest)
	{
		char c = *src;
		if (c == '\\')
			*dest = '/';
		else
			*dest = static_cast<char> (tolower (c));
	}

	*dest = '\0';

	bool deleted = true;
	if (strcmp (diskFileNameEntry, "deleted") != 0)
	{
		deleted = false;

		FILE *handle = safeFileOpen(diskFileNameEntry, "rb");

		if (!handle)
		{
			char warning [1024];
			sprintf (warning, "Unable to open data file %s\n", diskFileNameEntry);
			warnings.push_back (warning);
			return;
		}

		if (safeFileSeek(handle, 0, SEEK_END) != 0)
		{
			char warning [1024];
			sprintf (warning, "Unable to determine size of data file %s\n", diskFileNameEntry);
			warnings.push_back (warning);
			safeFileClose(handle);
			return;
		}

		long const fileSizeLong = safeFileTell(handle);
		if (fileSizeLong <= 0)
		{
			if (fileSizeLong == 0)
			{
				char warning [1024];
				sprintf (warning, "0-byte file detected %s\n", diskFileNameEntry);
				warnings.push_back (warning);
			}
			else
			{
				char warning [1024];
				sprintf (warning, "Unable to determine size of data file %s\n", diskFileNameEntry);
				warnings.push_back (warning);
			}
			deleted = true;
		}
		else if (fileSizeLong > static_cast<long>(INT_MAX))
		{
			char warning [1024];
			sprintf (warning, "Data file too large %s\n", diskFileNameEntry);
			warnings.push_back (warning);
			deleted = true;
		}

		safeFileClose(handle);
	}

	FileEntry* const newFileEntry = new FileEntry(diskFileNameEntry, lowerTreeFileName);
	newFileEntry->deleted = deleted;
	newFileEntry->uncompressed = uncompressedFile;

	std::vector<FileEntry*>::iterator iter;

	// set iterator to the first element whose Crc value is not less than newFileEntry
	iter = std::lower_bound(tocOrder.begin(), tocOrder.end(), newFileEntry, LessFileEntryCrcNameCompare);

	bool duplicateFileEntry = false;

	if (iter != tocOrder.end())
	{
		// an error results if adding two different diskFiles as the same treeFile
		if (newFileEntry->treeFileEntry == (*iter)->treeFileEntry && *(newFileEntry->diskFileEntry) != *((*iter)->diskFileEntry))
		{
			fprintf(stderr, "Unable to rename two different files with the same name: %s\n", (newFileEntry->treeFileEntry).getString());
			++errors;

			delete [] newFileEntry->diskFileEntry;
			delete newFileEntry;
			return;
		}
		else
			// check to see if the new file entry is a duplicate
			duplicateFileEntry = (newFileEntry->treeFileEntry == (*iter)->treeFileEntry);
	}

	if (duplicateFileEntry)
	{
		if (changingFileName)
			printf("*Duplicate file not added: %s[%s]\n", newFileEntry->treeFileEntry.getString(), newFileEntry->diskFileEntry);
		else
			printf("*Duplicate file not added: %s\n", newFileEntry->treeFileEntry.getString());

		++duplicateCount;

		delete [] newFileEntry->diskFileEntry;
		delete newFileEntry;
	}
	else
	{
		if (quiet < 1)
		{
			if (changingFileName)
				printf("Adding file %s[%s]\n", newFileEntry->treeFileEntry.getString(), newFileEntry->diskFileEntry);
			else
				printf("Adding file %s\n", newFileEntry->treeFileEntry.getString());
		}

		tocOrder.insert(iter, newFileEntry);
		responseFileOrder.insert(responseFileOrder.end(), newFileEntry);

		// allows a known creation size of array right before compression of name block
		uncompSizeOfNameBlock += (strlen(newFileEntry->treeFileEntry.getString()) + 1);
		++numberOfFiles;
	}
}

// ----------------------------------------------------------------------

void TreeFileBuilder::writeUnencrypted(const void *data, int length)
{
        DEBUG_FATAL(!treeFileHandle, ("Tree file handle is not valid"));

        if (length <= 0)
                return;

        size_t const wrote = safeFileWrite(data, static_cast<size_t>(length), treeFileHandle);
        DEBUG_FATAL(wrote != static_cast<size_t>(length), ("write failed"));
        static_cast<void>(wrote);
}

// ----------------------------------------------------------------------

void TreeFileBuilder::write(const void *data, int length)
{
        if (!encryptContent)
        {
                writeUnencrypted(data, length);
                return;
        }

        if (length <= 0)
                return;

        std::vector<unsigned char> buffer(static_cast<size_t>(length));
        std::memcpy(&buffer[0], data, static_cast<size_t>(length));
        TreeFileEncryption::transformBuffer(&buffer[0], length, encryptionKey, encryptionOffset);
        writeUnencrypted(&buffer[0], length);
        encryptionOffset += static_cast<uint32>(length);
}

// ----------------------------------------------------------------------

void TreeFileBuilder::writeFile(FileEntry *fileEntry)
{
	if (fileEntry->deleted)
	{
		if (quiet < 2)
			printf("  storing deleted file %s\n", fileEntry->treeFileEntry.getString());

		return;
	}

	const char *fileName = fileEntry->diskFileEntry;

	FILE *handle = safeFileOpen(fileName, "rb");
	FATAL(!handle, ("could not open file %s\n", fileName));

	if (safeFileSeek(handle, 0, SEEK_END) != 0)
	{
		safeFileClose(handle);
		FATAL(true, ("could not seek to end of file %s\n", fileName));
	}

	long const fileLengthLong = safeFileTell(handle);
	FATAL(fileLengthLong < 0 || fileLengthLong > static_cast<long>(INT_MAX), ("invalid length for file %s\n", fileName));

	if (safeFileSeek(handle, 0, SEEK_SET) != 0)
	{
		safeFileClose(handle);
		FATAL(true, ("could not seek to beginning of file %s\n", fileName));
	}

	int const fileLength = static_cast<int>(fileLengthLong);
	byte *uncompressed = NULL;

	if (fileLength > 0)
	{
		uncompressed = new byte[fileLength];
		size_t const bytesRead = safeFileRead(uncompressed, static_cast<size_t>(fileLength), handle);
		FATAL(bytesRead != static_cast<size_t>(fileLength), ("could not read file %s\n", fileName));
	}

	const int closeResult = safeFileClose(handle);
	FATAL(closeResult != 0, ("could not close file %s\n", fileName));

	// store the info about this file
	fileEntry->length     = fileLength;
	long const currentOffset = safeFileTell(treeFileHandle);
	FATAL(currentOffset < 0 || currentOffset > static_cast<long>(INT_MAX), ("tree file offset out of range"));
	fileEntry->offset     = static_cast<int>(currentOffset);

	// variable determining whether file compression is disabled or not
	bool disableCompression = (disableFileCompression || fileEntry->uncompressed);

	compressAndWrite(uncompressed, fileEntry->compressedLength, fileLength, fileEntry->compressor, true, disableCompression, &fileEntry->md5);
	delete [] uncompressed;
}

// ----------------------------------------------------------------------

void TreeFileBuilder::writeTableOfContents()
{
	TreeFile::SearchTree::TableOfContentsEntry  entry;

	int uncompSizeOfTOC = sizeof(entry) * numberOfFiles;
	int currentOffset   = 0;

	std::vector<FileEntry*>::iterator iter = tocOrder.begin();

	byte *uncompressed = new byte[uncompSizeOfTOC];
	byte *uncompIter   = uncompressed;

	// compress and write all of the file TOC info
	for (; iter != tocOrder.end(); ++iter, uncompIter += sizeof(entry))
	{
		// prepare entry by zeroing out the total size of data to be stored
		memset(&entry, 0, sizeof(entry));

		entry.crc                = (*iter)->treeFileEntry.getCrc();
		entry.length             = (*iter)->length;
		entry.offset             = (*iter)->offset;
		entry.compressor         = (*iter)->compressor;
		entry.compressedLength   = (*iter)->compressedLength;
		entry.fileNameOffset     = currentOffset;
		currentOffset           += strlen((*iter)->treeFileEntry.getString()) + 1;

		// copy the entry
		memcpy(uncompIter, &entry, sizeof(entry));
		fileNameBlock.push_back((*iter)->treeFileEntry.getString());
	}

	compressAndWrite(uncompressed, sizeOfTOC, uncompSizeOfTOC, tocCompressorID, false, disableTOCCompression, NULL);
	delete [] uncompressed;
}

// ----------------------------------------------------------------------

void TreeFileBuilder::writeFileNameBlock()
{
	byte *uncompressed = new byte[uncompSizeOfNameBlock];
	byte *uncompIter   = uncompressed;

	std::vector<const char *>::iterator iter = fileNameBlock.begin();

	for (int nameLength=0; iter != fileNameBlock.end(); ++iter)
	{
		nameLength = strlen(*iter) + 1;
		memcpy(uncompIter, *iter, nameLength);
		uncompIter += nameLength;
	}

	compressAndWrite(uncompressed, sizeOfNameBlock, uncompSizeOfNameBlock, blockCompressorID, false, disableTOCCompression, NULL);
	delete [] uncompressed;
}

// ----------------------------------------------------------------------

void TreeFileBuilder::writeMd5Block()
{
	int const sizeOfMd5Block = numberOfFiles * Md5::Value::cms_dataSize;
	byte *md5Block = new byte[sizeOfMd5Block];
	byte *destination = md5Block;

	// copy all of the md5 sums into a single block
	for (std::vector<FileEntry*>::iterator iter = tocOrder.begin(); iter != tocOrder.end(); ++iter, destination += Md5::Value::cms_dataSize)
		memcpy(destination, (*iter)->md5.getData(), Md5::Value::cms_dataSize);

	int compressedSize = 0;
	int compressor = 0;
	compressAndWrite(md5Block, compressedSize, sizeOfMd5Block, compressor, false, true, NULL);
	delete [] md5Block;
}

// ----------------------------------------------------------------------

void TreeFileBuilder::compressAndWrite(const byte *uncompressed, int &sizeOfData, const int uncompressedSize, int &compressor, const bool isFileData, const bool disableCompression, Md5::Value *md5)
{
    int smallest = TreeFile::SearchTree::CT_none;
    int smallestSize = uncompressedSize;
    byte *smallestBuffer = nullptr;
    TreeFileOutputType const outputType = getOutputType(treeFileName);
    bool const useBestCompression = (outputType == TFOT_tresx);

    if (!disableCompression && uncompressedSize > 1024)
    {
	TreeFile::SearchTree::CompressorType compressors[2];
	int numberOfCompressors = 0;
	compressors[numberOfCompressors++] = TreeFile::SearchTree::CT_zlib;
	if (useBestCompression)
		compressors[numberOfCompressors++] = TreeFile::SearchTree::CT_zlib_best;

	for (int i = 0; i < numberOfCompressors; ++i)
	{
	    TreeFile::SearchTree::CompressorType compressorType = compressors[i];

	    int newBufferLength = uncompressedSize * 2;
	    byte *newBuffer = new byte[newBufferLength];

	    Compressor *compressorInstance = nullptr;
	    switch (compressorType)
	    {
		case TreeFile::SearchTree::CT_zlib:
		    compressorInstance = new ZlibCompressor();
		    break;
		case TreeFile::SearchTree::CT_zlib_best:
		    compressorInstance = new ZlibCompressor(Z_BEST_COMPRESSION);
		    break;
		default:
		    break;
	    }

	    int size = -1;
	    if (compressorInstance)
	    {
		size = compressorInstance->compress(uncompressed, uncompressedSize, newBuffer, newBufferLength);
		delete compressorInstance;
	    }

	    if (size >= 0 && size < smallestSize)
	    {
		smallestSize = size;
		smallest = compressorType;

		delete [] smallestBuffer;
		smallestBuffer = newBuffer;
	    }
	    else
	    {
		delete [] newBuffer;
	    }
	}
    }

    if (isFileData)
    {
	if (TreeFile::SearchTree::isCompressed(smallest))
	{
	    if (quiet < 2)
		printf("  storing compressed(%d)   - OrigSize: %6d  CmpSize: %6d  Ratio: %3d%%\n", smallest, uncompressedSize, smallestSize, 100 - ((smallestSize * 100) / uncompressedSize));
	    sizeOfData = smallestSize;
	    write(smallestBuffer, smallestSize);
	}
	else
	{
	    sizeOfData = 0;
	    if (quiet < 2)
		printf("  storing uncompressed    - size: %6d\n", uncompressedSize);
	    write(uncompressed, uncompressedSize);
	}

	totalFileSize += uncompressedSize;
    }
    else
    {
	if (TreeFile::SearchTree::isCompressed(smallest))
	{
	    sizeOfData = smallestSize;
	    write(smallestBuffer, smallestSize);
	}
	else
	{
	    sizeOfData = uncompressedSize;
	    write(uncompressed, uncompressedSize);
	}

	totalFileSize += smallestSize;
    }

    if (md5)
    {
	if (TreeFile::SearchTree::isCompressed(smallest))
	    *md5 = Md5::calculate(smallestBuffer, smallestSize);
	else
	    *md5 = Md5::calculate(uncompressed, uncompressedSize);
    }

    totalSmallestSize += smallestSize;
    compressor = smallest;

    delete [] smallestBuffer;
}

// ----------------------------------------------------------------------

void TreeFileBuilder::write(void)
{
        // write the header
        TreeFile::SearchTree::Header header;
        TreeFileOutputType const outputType = getOutputType(treeFileName);
        if (outputType == TFOT_tresx)
                header.token = TAG_TRESX;
        else if (outputType == TFOT_tres)
                header.token = TAG_TRES;
        else
                header.token = TAG_TREE;
	header.version       = TAG_0005;
	header.numberOfFiles = numberOfFiles;
        writeUnencrypted(&header, sizeof(header));

        if (encryptContent)
                encryptionOffset = 0;

	std::vector<FileEntry*>::iterator iter = responseFileOrder.begin();

	printf("Generating tree file body\n");

	// write all of the files
	const uint total = responseFileOrder.size ();
	for(int current = 1; iter != responseFileOrder.end(); ++iter, ++current)
	{
		if (quiet < 2)
			printf("[%4i/%4i] ", current, total);
		writeFile(*iter);
	}

	// calculate the tocOffset before writing the TOC
	int tocOffset = totalSmallestSize + sizeof(header);

	printf("Writing table of contents\n");
	writeTableOfContents();
	writeFileNameBlock();

	printf("Writing md5sum block\n");
	writeMd5Block();

	// now go back and update the header info with the correct data
	DEBUG_FATAL(safeFileSeek(treeFileHandle, 0, SEEK_SET) != 0, ("failed to seek to beginning of tree file"));
	header.tocOffset		     = tocOffset;
	header.tocCompressor	     = tocCompressorID;
	header.sizeOfTOC		 = sizeOfTOC;
	header.blockCompressor       = blockCompressorID;
	header.sizeOfNameBlock		 = sizeOfNameBlock;
	header.uncompSizeOfNameBlock = uncompSizeOfNameBlock;
        writeUnencrypted(&header, sizeof(header));

	if(totalFileSize == totalSmallestSize)
		printf("Final compression: OrigSize: %6d  CmpSize: %6d  Ratio: %3d%%\n", totalFileSize,
			    totalSmallestSize, 0);
	else
	{
		if (totalFileSize > 1024 * 1024)
		{
			const int totalFileSizeMegaBytes = totalFileSize /  (1024 * 1024);
			const int totalSmallestSizeMegaBytes = totalSmallestSize / (1024 * 1024);
			printf("Final compression: OrigSize: %6dMB CmpSize: %6dMB  Ratio: %3d%%\n", totalFileSizeMegaBytes,
					totalSmallestSizeMegaBytes, 100 - ((totalSmallestSizeMegaBytes * 100) / totalFileSizeMegaBytes));
		}
		else
                {
                        printf("Final compression: OrigSize: %6d  CmpSize: %6d  Ratio: %3d%%\n", totalFileSize,
                                        totalSmallestSize, 100 - ((totalSmallestSize * 100) / totalFileSize));
                }
        }
}

// ======================================================================

void TreeFileBuilder::enableEncryption(const char *passphrase)
{
        if (TreeFileEncryption::isPassphraseValid(passphrase))
        {
                encryptionKey = TreeFileEncryption::deriveKey(passphrase);
                encryptContent = true;
                encryptionOffset = 0;
        }
        else
        {
                disableEncryption();
        }
}

// ----------------------------------------------------------------------

void TreeFileBuilder::disableEncryption()
{
        encryptContent = false;
        encryptionOffset = 0;
}

// ----------------------------------------------------------------------

bool TreeFileBuilder::isEncryptionEnabled() const
{
        return encryptContent;
}

// ----------------------------------------------------------------------

bool TreeFileBuilder::encryptTreeFile(const char *sourceFileName, const char *destinationFileName, const char *passphrase)
{
        if (!TreeFileEncryption::isPassphraseValid(passphrase))
        {
                fprintf(stderr, "Encryption requires a passphrase value.\n");
                return false;
        }

        Md5::Value const key = TreeFileEncryption::deriveKey(passphrase);
        return transformTreeFile(sourceFileName, destinationFileName, key, TM_encrypt);
}

// ----------------------------------------------------------------------

bool TreeFileBuilder::decryptTreeFile(const char *sourceFileName, const char *destinationFileName, const char *passphrase)
{
        if (!TreeFileEncryption::isPassphraseValid(passphrase))
        {
                fprintf(stderr, "Decryption requires a passphrase value.\n");
                return false;
        }

        Md5::Value const key = TreeFileEncryption::deriveKey(passphrase);
        return transformTreeFile(sourceFileName, destinationFileName, key, TM_decrypt);
}

// ======================================================================

// ======================================================================
