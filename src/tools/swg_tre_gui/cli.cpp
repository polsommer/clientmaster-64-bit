#include "TreArchive.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

namespace {
void print_usage(const char *exe) {
    std::cout << "Usage: " << exe << " <input.tre|input.tres> <output.tre|output.tres> [--passphrase <text>]" << std::endl;
    std::cout << "Convert between TRE and encrypted TRES archives using the C++ toolchain." << std::endl;
}
}

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string input(argv[1]);
    const std::string output(argv[2]);

    const auto hasTresExtension = [](const std::string &path) {
        if (path.size() < 5) {
            return false;
        }
        const std::string tail = path.substr(path.size() - 4);
        std::string lower;
        lower.resize(tail.size());
        std::transform(tail.begin(), tail.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return lower == ".tres";
    };

    std::string passphrase;
    for (int i = 3; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--passphrase" && i + 1 < argc) {
            passphrase = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    const bool input_encrypted = hasTresExtension(input);
    const bool output_encrypted = hasTresExtension(output);

    if ((input_encrypted || output_encrypted) && passphrase.empty()) {
        std::cerr << "A non-empty passphrase is required when working with encrypted .tres files." << std::endl;
        return 1;
    }

    try {
        TreArchive archive = TreArchive::load(input, passphrase);
        archive.save(output, output_encrypted ? passphrase : std::string());
    } catch (const std::exception &err) {
        std::cerr << "Failed to convert archive: " << err.what() << std::endl;
        return 1;
    }

    std::cout << "Wrote " << output << " from " << input << std::endl;
    return 0;
}
