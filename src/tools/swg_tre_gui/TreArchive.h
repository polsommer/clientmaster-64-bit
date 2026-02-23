#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

class TreArchiveError : public std::runtime_error {
public:
    explicit TreArchiveError(const std::string &message) : std::runtime_error(message) {}
    explicit TreArchiveError(const char *message) : std::runtime_error(message) {}
};

class TreArchive {
public:
    struct Entry {
        std::string name;
        std::vector<std::uint8_t> data;
        bool uncompressed;
    };

    TreArchive() = default;

    static TreArchive load(const std::string &path, const std::string &passphrase = {});

    void add_bytes(const std::string &archive_name, std::vector<std::uint8_t> bytes, bool store_uncompressed = false);

    void add_file(const std::string &disk_path, const std::string &archive_name);
    void remove_entry(std::size_t index);

    void save(const std::string &path, const std::string &passphrase = {}) const;

    const std::vector<Entry> &entries() const { return m_entries; }
    bool empty() const { return m_entries.empty(); }

private:
    std::vector<Entry> m_entries;
};

std::string format_bytes(const std::vector<std::uint8_t> &data);
