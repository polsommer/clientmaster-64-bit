#pragma once

#include "Json.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class IffDefinitionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class IffNode {
public:
    virtual ~IffNode() = default;
    virtual std::vector<std::uint8_t> to_bytes() const = 0;
    virtual std::string describe(std::size_t indent = 0) const = 0;
};

class IffChunk : public IffNode {
public:
    IffChunk(std::array<char, 4> tag, std::vector<std::uint8_t> data);

    std::vector<std::uint8_t> to_bytes() const override;
    std::string describe(std::size_t indent) const override;

private:
    std::array<char, 4> m_tag;
    std::vector<std::uint8_t> m_data;
};

class IffForm : public IffNode {
public:
    IffForm(std::array<char, 4> tag, std::vector<std::unique_ptr<IffNode>> children);

    std::vector<std::uint8_t> to_bytes() const override;
    std::string describe(std::size_t indent) const override;

private:
    std::array<char, 4> m_tag;
    std::vector<std::unique_ptr<IffNode>> m_children;
};

class IffBuilder {
public:
    explicit IffBuilder(std::unique_ptr<IffNode> root);

    static IffBuilder from_definition(const JsonValue &definition);

    std::vector<std::uint8_t> build_bytes() const;
    void write(const std::filesystem::path &path) const;
    std::string describe() const;

private:
    std::unique_ptr<IffNode> m_root;
};

std::array<char, 4> parse_tag(const JsonValue &value);
std::unique_ptr<IffNode> parse_node(const JsonValue &definition);
std::vector<std::uint8_t> coerce_data(const JsonValue &value, const JsonValue *encoding_value);
std::vector<std::uint8_t> pad_even(const std::vector<std::uint8_t> &payload);
