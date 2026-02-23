#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

class JsonParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class JsonValue {
public:
    using object_type = std::map<std::string, JsonValue, std::less<>>;
    using array_type = std::vector<JsonValue>;

    JsonValue();
    explicit JsonValue(std::nullptr_t);
    explicit JsonValue(bool value);
    explicit JsonValue(double value);
    explicit JsonValue(std::string value);
    explicit JsonValue(array_type value);
    explicit JsonValue(object_type value);

    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    bool as_bool() const;
    double as_number() const;
    const std::string &as_string() const;
    const array_type &as_array() const;
    const object_type &as_object() const;

    const JsonValue *find(const std::string &key) const;

    std::string to_compact_string() const;

private:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type m_type;
    bool m_bool_value{false};
    double m_number_value{0.0};
    std::string m_string_value;
    array_type m_array_value;
    object_type m_object_value;
};

JsonValue parse_json(const std::string &text);
