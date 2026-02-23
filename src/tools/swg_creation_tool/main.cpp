#include "IffBuilder.h"
#include "Json.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_usage() {
    std::cout << "swg+creation_tool - build IFF assets from JSON definitions\n"
              << "Usage: swg+creation_tool --input <definition.json> --output <file.iff> [--describe]\n\n"
              << "  --input <path>     Path to the JSON definition file\n"
              << "  --output <path>    Destination for the generated IFF file\n"
              << "  --describe         Print the IFF layout to stdout before writing\n";
}

std::string read_text_file(const std::filesystem::path &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Unable to open input file: " + path.string());
    }
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return contents;
}

} // namespace

int main(int argc, char **argv) {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    bool describe = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
            continue;
        }
        if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
            continue;
        }
        if (arg == "--describe") {
            describe = true;
            continue;
        }
        print_usage();
        return 1;
    }

    if (input_path.empty() || output_path.empty()) {
        print_usage();
        return 1;
    }

    try {
        std::string definition_text = read_text_file(input_path);
        JsonValue definition = parse_json(definition_text);
        IffBuilder builder = IffBuilder::from_definition(definition);

        if (describe) {
            std::cout << builder.describe() << "\n";
        }

        builder.write(output_path);
        return 0;
    } catch (const JsonParseError &err) {
        std::cerr << "Failed to parse JSON: " << err.what() << "\n";
    } catch (const IffDefinitionError &err) {
        std::cerr << "Invalid IFF definition: " << err.what() << "\n";
    } catch (const std::exception &err) {
        std::cerr << "Unexpected error: " << err.what() << "\n";
    }

    return 1;
}
