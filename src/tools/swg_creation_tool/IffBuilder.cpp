#include "IffBuilder.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::array<char, 4> to_tag(const std::string &text) {
    if (text.size() != 4) {
        throw IffDefinitionError("IFF tags must be exactly four characters long");
    }
    std::array<char, 4> tag{};
    std::copy(text.begin(), text.end(), tag.begin());
    return tag;
}

std::vector<std::uint8_t> to_big_endian(std::uint32_t value) {
    std::vector<std::uint8_t> bytes(4);
    bytes[0] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    bytes[1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    bytes[2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    bytes[3] = static_cast<std::uint8_t>(value & 0xFF);
    return bytes;
}

std::vector<std::uint8_t> encode_string(const std::string &text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

std::vector<std::uint8_t> decode_hex(const std::string &text) {
    if (text.size() % 2 != 0) {
        throw IffDefinitionError("Hex payloads must contain an even number of characters");
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        int hi = hex_value(text[i]);
        int lo = hex_value(text[i + 1]);
        if (hi < 0 || lo < 0) {
            throw IffDefinitionError("Invalid hex digit in payload");
        }
        bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return bytes;
}

int base64_value(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z') {
        return 26 + (ch - 'a');
    }
    if (ch >= '0' && ch <= '9') {
        return 52 + (ch - '0');
    }
    if (ch == '+') {
        return 62;
    }
    if (ch == '/') {
        return 63;
    }
    if (ch == '=') {
        return -2; // padding
    }
    return -1;
}

std::vector<std::uint8_t> decode_base64(const std::string &text) {
    std::vector<std::uint8_t> result;
    std::uint32_t buffer = 0;
    int bits_collected = 0;

    for (char ch : text) {
        int value = base64_value(ch);
        if (value < -1) {
            throw IffDefinitionError("Invalid base64 character in payload");
        }
        if (value == -2) {
            break; // padding
        }
        if (value >= 0) {
            buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
            bits_collected += 6;
            if (bits_collected >= 8) {
                bits_collected -= 8;
                std::uint8_t byte = static_cast<std::uint8_t>((buffer >> bits_collected) & 0xFFu);
                result.push_back(byte);
            }
        }
    }

    return result;
}

bool is_integer_value(double value) {
    return std::floor(value) == value;
}

std::vector<std::uint8_t> encode_json_value(const JsonValue &value) {
    std::string text = value.to_compact_string();
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

} // namespace

IffChunk::IffChunk(std::array<char, 4> tag, std::vector<std::uint8_t> data)
    : m_tag(tag), m_data(std::move(data)) {}

std::vector<std::uint8_t> IffChunk::to_bytes() const {
    std::vector<std::uint8_t> result;
    auto size_bytes = to_big_endian(static_cast<std::uint32_t>(m_data.size()));
    auto payload = pad_even(m_data);

    result.insert(result.end(), m_tag.begin(), m_tag.end());
    result.insert(result.end(), size_bytes.begin(), size_bytes.end());
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::string IffChunk::describe(std::size_t indent) const {
    std::string prefix(indent, ' ');
    return prefix + "CHUNK " + std::string(m_tag.begin(), m_tag.end()) + " (" +
           std::to_string(m_data.size()) + " bytes)";
}

IffForm::IffForm(std::array<char, 4> tag, std::vector<std::unique_ptr<IffNode>> children)
    : m_tag(tag), m_children(std::move(children)) {}

std::vector<std::uint8_t> IffForm::to_bytes() const {
    std::vector<std::uint8_t> child_bytes;
    for (const auto &child : m_children) {
        auto bytes = child->to_bytes();
        child_bytes.insert(child_bytes.end(), bytes.begin(), bytes.end());
    }

    auto payload = pad_even(child_bytes);
    auto size_bytes = to_big_endian(static_cast<std::uint32_t>(child_bytes.size() + 4));

    std::vector<std::uint8_t> result;
    const std::array<char, 4> header_tag{'F', 'O', 'R', 'M'};
    result.insert(result.end(), header_tag.begin(), header_tag.end());
    result.insert(result.end(), size_bytes.begin(), size_bytes.end());
    result.insert(result.end(), m_tag.begin(), m_tag.end());
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::string IffForm::describe(std::size_t indent) const {
    std::string prefix(indent, ' ');
    std::ostringstream out;
    out << prefix << "FORM " << std::string(m_tag.begin(), m_tag.end());
    if (m_children.empty()) {
        out << " (empty)";
        return out.str();
    }
    for (const auto &child : m_children) {
        out << '\n' << child->describe(indent + 2);
    }
    return out.str();
}

IffBuilder::IffBuilder(std::unique_ptr<IffNode> root) : m_root(std::move(root)) {}

IffBuilder IffBuilder::from_definition(const JsonValue &definition) {
    return IffBuilder(parse_node(definition));
}

std::vector<std::uint8_t> IffBuilder::build_bytes() const { return m_root->to_bytes(); }

void IffBuilder::write(const std::filesystem::path &path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open output path: " + path.string());
    }
    const auto bytes = build_bytes();
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::string IffBuilder::describe() const { return m_root->describe(); }

std::array<char, 4> parse_tag(const JsonValue &value) {
    if (!value.is_string()) {
        throw IffDefinitionError("IFF tags must be strings");
    }
    return to_tag(value.as_string());
}

std::unique_ptr<IffNode> parse_node(const JsonValue &definition) {
    if (!definition.is_object()) {
        throw IffDefinitionError("IFF definition must be a JSON object");
    }
    const auto &obj = definition.as_object();

    auto chunk_it = obj.find("chunk");
    if (chunk_it != obj.end()) {
        const JsonValue &tag_value = chunk_it->second;
        std::vector<std::uint8_t> data;
        auto data_it = obj.find("data");
        if (data_it != obj.end()) {
            const JsonValue *encoding = nullptr;
            auto encoding_it = obj.find("encoding");
            if (encoding_it != obj.end()) {
                encoding = &encoding_it->second;
            }
            data = coerce_data(data_it->second, encoding);
        }
        return std::make_unique<IffChunk>(parse_tag(tag_value), std::move(data));
    }

    auto form_it = obj.find("form");
    if (form_it != obj.end()) {
        const JsonValue &tag_value = form_it->second;
        std::vector<std::unique_ptr<IffNode>> children;
        auto children_it = obj.find("children");
        if (children_it != obj.end()) {
            if (!children_it->second.is_array()) {
                throw IffDefinitionError("FORM children must be an array");
            }
            for (const auto &entry : children_it->second.as_array()) {
                children.push_back(parse_node(entry));
            }
        }
        return std::make_unique<IffForm>(parse_tag(tag_value), std::move(children));
    }

    throw IffDefinitionError("Definition must include either 'chunk' or 'form'");
}

std::vector<std::uint8_t> coerce_data(const JsonValue &value, const JsonValue *encoding_value) {
    std::string encoding;
    if (encoding_value != nullptr) {
        if (!encoding_value->is_string()) {
            throw IffDefinitionError("encoding must be a string when provided");
        }
        encoding = encoding_value->as_string();
    }

    if (value.is_string()) {
        if (encoding.empty() || encoding == "text") {
            return encode_string(value.as_string());
        }
        if (encoding == "hex") {
            return decode_hex(value.as_string());
        }
        if (encoding == "base64") {
            return decode_base64(value.as_string());
        }
        throw IffDefinitionError("Unknown encoding for string payloads: " + encoding);
    }

    if (value.is_array()) {
        const auto &array = value.as_array();
        if (std::all_of(array.begin(), array.end(), [](const JsonValue &entry) {
                return entry.is_number() && is_integer_value(entry.as_number()) && entry.as_number() >= 0.0 && entry.as_number() <= 255.0;
            })) {
            std::vector<std::uint8_t> bytes;
            bytes.reserve(array.size());
            for (const auto &entry : array) {
                bytes.push_back(static_cast<std::uint8_t>(entry.as_number()));
            }
            return bytes;
        }
        return encode_json_value(value);
    }

    if (value.is_object() || value.is_bool() || value.is_number()) {
        return encode_json_value(value);
    }

    if (value.is_null()) {
        return {};
    }

    throw IffDefinitionError("Unsupported data type for chunk payload");
}

std::vector<std::uint8_t> pad_even(const std::vector<std::uint8_t> &payload) {
    if (payload.size() % 2 == 0) {
        return payload;
    }
    auto padded = payload;
    padded.push_back(0);
    return padded;
}
