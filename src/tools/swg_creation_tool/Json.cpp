#include "Json.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace {

class Parser {
public:
    explicit Parser(const std::string &input) : m_input(input) {}

    JsonValue parse() {
        skip_whitespace();
        JsonValue value = parse_value();
        skip_whitespace();
        if (!eof()) {
            throw JsonParseError("Unexpected trailing characters in JSON text");
        }
        return value;
    }

private:
    JsonValue parse_value() {
        if (match("null")) {
            return JsonValue(nullptr);
        }
        if (match("true")) {
            return JsonValue(true);
        }
        if (match("false")) {
            return JsonValue(false);
        }
        if (peek() == '"') {
            return JsonValue(parse_string());
        }
        if (peek() == '{') {
            return parse_object();
        }
        if (peek() == '[') {
            return parse_array();
        }
        if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) {
            return JsonValue(parse_number());
        }

        throw JsonParseError("Unexpected token in JSON input");
    }

    JsonValue parse_object() {
        expect('{');
        JsonValue::object_type result;
        skip_whitespace();
        if (peek() == '}') {
            advance();
            return JsonValue(std::move(result));
        }

        while (true) {
            skip_whitespace();
            if (peek() != '"') {
                throw JsonParseError("Expected string key in object");
            }
            std::string key = parse_string();
            skip_whitespace();
            expect(':');
            skip_whitespace();
            result.emplace(std::move(key), parse_value());
            skip_whitespace();
            char ch = peek();
            if (ch == ',') {
                advance();
                continue;
            }
            if (ch == '}') {
                advance();
                break;
            }
            throw JsonParseError("Expected ',' or '}' in object");
        }

        return JsonValue(std::move(result));
    }

    JsonValue parse_array() {
        expect('[');
        JsonValue::array_type values;
        skip_whitespace();
        if (peek() == ']') {
            advance();
            return JsonValue(std::move(values));
        }

        while (true) {
            skip_whitespace();
            values.push_back(parse_value());
            skip_whitespace();
            char ch = peek();
            if (ch == ',') {
                advance();
                continue;
            }
            if (ch == ']') {
                advance();
                break;
            }
            throw JsonParseError("Expected ',' or ']' in array");
        }

        return JsonValue(std::move(values));
    }

    std::string parse_string() {
        expect('"');
        std::string value;
        while (!eof()) {
            char ch = advance();
            if (ch == '"') {
                break;
            }
            if (ch == '\\') {
                value += parse_escape_sequence();
                continue;
            }
            value.push_back(ch);
        }
        return value;
    }

    char32_t parse_hex_codepoint() {
        if (m_pos + 4 > m_input.size()) {
            throw JsonParseError("Incomplete unicode escape sequence");
        }
        char32_t codepoint = 0;
        for (int i = 0; i < 4; ++i) {
            char ch = m_input[m_pos + static_cast<size_t>(i)];
            codepoint <<= 4;
            if (ch >= '0' && ch <= '9') {
                codepoint |= static_cast<char32_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                codepoint |= static_cast<char32_t>(10 + ch - 'a');
            } else if (ch >= 'A' && ch <= 'F') {
                codepoint |= static_cast<char32_t>(10 + ch - 'A');
            } else {
                throw JsonParseError("Invalid hex digit in unicode escape");
            }
        }
        m_pos += 4;
        return codepoint;
    }

    std::string encode_codepoint(char32_t codepoint) {
        std::string out;
        if (codepoint <= 0x7F) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        return out;
    }

    char parse_escape_sequence() {
        if (eof()) {
            throw JsonParseError("Unterminated escape sequence");
        }
        char ch = advance();
        switch (ch) {
        case '"':
        case '\\':
        case '/':
            return ch;
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'u': {
            char32_t codepoint = parse_hex_codepoint();
            // Handle surrogate pairs
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                if (!(peek() == '\\' && m_pos + 1 < m_input.size() && m_input[m_pos + 1] == 'u')) {
                    throw JsonParseError("Invalid unicode surrogate pair");
                }
                m_pos += 2; // consume \u
                char32_t trailing = parse_hex_codepoint();
                if (trailing < 0xDC00 || trailing > 0xDFFF) {
                    throw JsonParseError("Invalid unicode surrogate pair");
                }
                codepoint = ((codepoint - 0xD800) << 10) + (trailing - 0xDC00) + 0x10000;
            }
            std::string encoded = encode_codepoint(codepoint);
            if (encoded.size() != 1) {
                m_pending_utf8 = encoded;
                return pop_pending_utf8();
            }
            return encoded.front();
        }
        default:
            throw JsonParseError("Unknown escape sequence");
        }
    }

    double parse_number() {
        size_t start = m_pos;
        if (peek() == '-') {
            advance();
        }
        if (peek() == '0') {
            advance();
        } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        } else {
            throw JsonParseError("Invalid number");
        }

        if (peek() == '.') {
            advance();
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                throw JsonParseError("Invalid fractional number");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }

        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                throw JsonParseError("Invalid exponent");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }

        std::string number_view = m_input.substr(start, m_pos - start);
        char *end_ptr = nullptr;
        double result = std::strtod(number_view.c_str(), &end_ptr);
        if (end_ptr == nullptr || static_cast<size_t>(end_ptr - number_view.c_str()) != number_view.size()) {
            throw JsonParseError("Failed to parse number");
        }
        return result;
    }

    void skip_whitespace() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    void expect(char expected) {
        if (peek() != expected) {
            throw JsonParseError("Unexpected character in JSON input");
        }
        advance();
    }

    bool match(const std::string &text) {
        if (m_input.substr(m_pos, text.size()) == text) {
            m_pos += text.size();
            return true;
        }
        return false;
    }

    bool eof() const { return m_pos >= m_input.size(); }

    char peek() const { return eof() ? '\0' : m_input[m_pos]; }

    char advance() {
        if (m_pending_utf8.empty()) {
            if (eof()) {
                throw JsonParseError("Unexpected end of input");
            }
            return m_input[m_pos++];
        }
        return pop_pending_utf8();
    }

    char pop_pending_utf8() {
        char ch = m_pending_utf8.front();
        m_pending_utf8.erase(m_pending_utf8.begin());
        return ch;
    }

    const std::string &m_input;
    size_t m_pos{0};
    std::string m_pending_utf8;
};

std::string value_to_string(const JsonValue &value);
std::string object_to_string(const JsonValue::object_type &object);
std::string array_to_string(const JsonValue::array_type &array);

std::string value_to_string(const JsonValue &value) {
    if (value.is_null()) {
        return "null";
    }
    if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    if (value.is_number()) {
        std::ostringstream out;
        out << value.as_number();
        return out.str();
    }
    if (value.is_string()) {
        std::ostringstream out;
        out << '"';
        for (char ch : value.as_string()) {
            switch (ch) {
            case '\\':
            case '"':
                out << '\\' << ch;
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << ch;
                break;
            }
        }
        out << '"';
        return out.str();
    }
    if (value.is_array()) {
        return array_to_string(value.as_array());
    }
    return object_to_string(value.as_object());
}

std::string object_to_string(const JsonValue::object_type &object) {
    std::ostringstream out;
    out << '{';
    bool first = true;
    for (const auto &entry : object) {
        if (!first) {
            out << ',';
        }
        first = false;
        out << '"' << entry.first << '"' << ':' << value_to_string(entry.second);
    }
    out << '}';
    return out.str();
}

std::string array_to_string(const JsonValue::array_type &array) {
    std::ostringstream out;
    out << '[';
    bool first = true;
    for (const auto &value : array) {
        if (!first) {
            out << ',';
        }
        first = false;
        out << value_to_string(value);
    }
    out << ']';
    return out.str();
}

} // namespace

JsonValue::JsonValue() : m_type(Type::Null) {}
JsonValue::JsonValue(std::nullptr_t) : m_type(Type::Null) {}
JsonValue::JsonValue(bool value) : m_type(Type::Bool), m_bool_value(value) {}
JsonValue::JsonValue(double value) : m_type(Type::Number), m_number_value(value) {}
JsonValue::JsonValue(std::string value) : m_type(Type::String), m_string_value(std::move(value)) {}
JsonValue::JsonValue(array_type value) : m_type(Type::Array), m_array_value(std::move(value)) {}
JsonValue::JsonValue(object_type value) : m_type(Type::Object), m_object_value(std::move(value)) {}

bool JsonValue::is_null() const { return m_type == Type::Null; }
bool JsonValue::is_bool() const { return m_type == Type::Bool; }
bool JsonValue::is_number() const { return m_type == Type::Number; }
bool JsonValue::is_string() const { return m_type == Type::String; }
bool JsonValue::is_array() const { return m_type == Type::Array; }
bool JsonValue::is_object() const { return m_type == Type::Object; }

bool JsonValue::as_bool() const { return m_bool_value; }
double JsonValue::as_number() const { return m_number_value; }
const std::string &JsonValue::as_string() const { return m_string_value; }
const JsonValue::array_type &JsonValue::as_array() const { return m_array_value; }
const JsonValue::object_type &JsonValue::as_object() const { return m_object_value; }

const JsonValue *JsonValue::find(const std::string &key) const {
    if (!is_object()) {
        return nullptr;
    }
    const auto &obj = as_object();
    auto it = obj.find(std::string(key));
    if (it == obj.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string JsonValue::to_compact_string() const { return value_to_string(*this); }

JsonValue parse_json(const std::string &text) {
    Parser parser(text);
    return parser.parse();
}
