#include "TreArchive.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#if defined(__has_include)
#if __has_include(<zlib.h>)
#include <zlib.h>
#elif __has_include("../../external/3rd/library/zlib/include/zlib.h")
#include "../../external/3rd/library/zlib/include/zlib.h"
#else
#error "zlib.h not found"
#endif
#else
#include "../../external/3rd/library/zlib/include/zlib.h"
#endif

namespace {
std::uint32_t left_rotate(std::uint32_t value, std::uint32_t amount) {
    return (value << amount) | (value >> (32U - amount));
}

std::array<std::uint8_t, 16> md5_digest(const std::string &input) {
    static constexpr std::uint32_t K[] = {
        0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
        0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU, 0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
        0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU, 0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
        0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
        0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU, 0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
        0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U, 0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
        0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
        0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U, 0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
    };

    static constexpr std::uint32_t S[] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20,
        5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    };

    std::vector<std::uint8_t> message(input.begin(), input.end());
    message.push_back(0x80U);
    while ((message.size() % 64U) != 56U) {
        message.push_back(0x00U);
    }

    const std::uint64_t bit_length = static_cast<std::uint64_t>(input.size()) * 8ULL;
    for (int i = 0; i < 8; ++i) {
        message.push_back(static_cast<std::uint8_t>((bit_length >> (8 * i)) & 0xFFU));
    }

    std::uint32_t a0 = 0x67452301U;
    std::uint32_t b0 = 0xefcdab89U;
    std::uint32_t c0 = 0x98badcfeU;
    std::uint32_t d0 = 0x10325476U;

    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        std::uint32_t M[16];
        for (int i = 0; i < 16; ++i) {
            const std::size_t index = offset + static_cast<std::size_t>(i * 4);
            M[i] = static_cast<std::uint32_t>(message[index]) |
                   (static_cast<std::uint32_t>(message[index + 1]) << 8U) |
                   (static_cast<std::uint32_t>(message[index + 2]) << 16U) |
                   (static_cast<std::uint32_t>(message[index + 3]) << 24U);
        }

        std::uint32_t A = a0;
        std::uint32_t B = b0;
        std::uint32_t C = c0;
        std::uint32_t D = d0;

        for (int i = 0; i < 64; ++i) {
            std::uint32_t F;
            int g;

            if (i < 16) {
                F = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                F = (D & B) | (~D & C);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                F = B ^ C ^ D;
                g = (3 * i + 5) % 16;
            } else {
                F = C ^ (B | ~D);
                g = (7 * i) % 16;
            }

            const std::uint32_t temp = D;
            D = C;
            C = B;
            B = B + left_rotate(A + F + K[i] + M[g], S[i]);
            A = temp;
        }

        a0 += A;
        b0 += B;
        c0 += C;
        d0 += D;
    }

    std::array<std::uint8_t, 16> digest{};
    const auto write_word = [&digest](std::uint32_t value, int offset) {
        digest[static_cast<std::size_t>(offset)] = static_cast<std::uint8_t>(value & 0xFFU);
        digest[static_cast<std::size_t>(offset + 1)] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        digest[static_cast<std::size_t>(offset + 2)] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
        digest[static_cast<std::size_t>(offset + 3)] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    };

    write_word(a0, 0);
    write_word(b0, 4);
    write_word(c0, 8);
    write_word(d0, 12);

    return digest;
}

constexpr std::uint32_t TAG_TREE = 0x54524545; // "TREE"
constexpr std::uint32_t TAG_TRES = 0x54524553; // "TRES"
constexpr std::uint32_t TAG_0004 = 0x30303034; // "0004"
constexpr std::uint32_t TAG_0005 = 0x30303035; // "0005"

enum class Compressor : std::uint32_t {
    None = 0,
    Deprecated = 1,
    Zlib = 2,
};

struct Header {
    std::uint32_t token;
    std::uint32_t version;
    std::uint32_t number_of_files;
    std::uint32_t toc_offset;
    std::uint32_t toc_compressor;
    std::uint32_t toc_size;
    std::uint32_t name_block_compressor;
    std::uint32_t name_block_size;
    std::uint32_t name_block_uncompressed_size;
};

struct TocEntry {
    std::uint32_t crc;
    std::uint32_t length;
    std::uint32_t offset;
    std::uint32_t compressor;
    std::uint32_t compressed_length;
    std::uint32_t file_name_offset;
};

std::uint32_t read_u32(std::istream &stream) {
    std::uint32_t value = 0;
    stream.read(reinterpret_cast<char *>(&value), sizeof(value));
    if (!stream) {
        throw TreArchiveError("Unexpected end of file while reading u32");
    }
    return value;
}

void write_u32(std::ostream &stream, std::uint32_t value) {
    stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
    if (!stream) {
        throw TreArchiveError("Failed while writing archive payload");
    }
}

std::array<std::uint8_t, 16> derive_key(const std::string &passphrase) {
    std::array<std::uint8_t, 16> key{};
    key = md5_digest(passphrase);
    return key;
}

void transform_buffer(std::vector<std::uint8_t> &buffer, const std::array<std::uint8_t, 16> &key, std::uint32_t start_offset) {
    if (buffer.empty()) {
        return;
    }

    const std::size_t key_length = key.size();
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        const std::size_t key_index = (start_offset + i) % key_length;
        buffer[i] ^= key[key_index];
    }
}

std::vector<std::uint8_t> read_file_bytes(const std::string &path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        throw TreArchiveError("Unable to open file: " + path);
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::uint32_t crc_string(const std::string &text) {
    static constexpr std::uint32_t CRC_INIT = 0xFFFFFFFFu;
    static constexpr std::uint32_t TABLE[256] = {
#include "../swg_creation_tool/CrcTable.inl"
    };

    std::uint32_t crc = CRC_INIT;
    for (unsigned char ch : text) {
        crc = TABLE[((crc >> 24) ^ ch) & 0xFFu] ^ (crc << 8);
    }
    return (crc ^ CRC_INIT);
}

std::vector<std::uint8_t> zlib_decompress(const std::vector<std::uint8_t> &data, std::size_t expected) {
    std::vector<std::uint8_t> out(expected);
    uLongf dest_len = static_cast<uLongf>(expected);
    const int result = uncompress(out.data(), &dest_len, data.data(), static_cast<uLong>(data.size()));
    if (result != Z_OK || dest_len != expected) {
        throw TreArchiveError("Failed to decompress zlib block");
    }
    return out;
}

std::vector<std::uint8_t> zlib_compress(const std::vector<std::uint8_t> &data) {
    uLongf dest_len = compressBound(static_cast<uLong>(data.size()));
    std::vector<std::uint8_t> compressed(dest_len);
    const int result = compress2(compressed.data(), &dest_len, data.data(), static_cast<uLong>(data.size()), Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        throw TreArchiveError("Failed to compress data block");
    }
    compressed.resize(dest_len);
    return compressed;
}

std::string normalize_name(const std::string &name) {
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lower;
}

} // namespace

TreArchive TreArchive::load(const std::string &path, const std::string &passphrase) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        throw TreArchiveError("Unable to open archive: " + path);
    }

    Header header{};
    header.token = read_u32(in);
    header.version = read_u32(in);
    header.number_of_files = read_u32(in);
    header.toc_offset = read_u32(in);
    header.toc_compressor = read_u32(in);
    header.toc_size = read_u32(in);
    header.name_block_compressor = read_u32(in);
    header.name_block_size = read_u32(in);
    header.name_block_uncompressed_size = read_u32(in);

    const bool encrypted = header.token == TAG_TRES;
    if (encrypted && passphrase.empty()) {
        throw TreArchiveError("Encrypted TRES archives require a passphrase");
    }
    if (header.token != TAG_TREE && !encrypted) {
        throw TreArchiveError("Archive is missing TREE header");
    }
    std::array<std::uint8_t, 16> key{};
    if (encrypted) {
        key = derive_key(passphrase);
    }
    if (header.version != TAG_0004 && header.version != TAG_0005) {
        throw TreArchiveError("Unsupported TREE version");
    }

    const auto read_block = [&](std::uint32_t offset, std::size_t size) {
        std::vector<std::uint8_t> buffer(size);
        in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        in.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        if (!in) {
            throw TreArchiveError("Failed to read encrypted archive segment");
        }
        if (encrypted) {
            const std::uint32_t transform_offset = offset - static_cast<std::uint32_t>(sizeof(Header));
            transform_buffer(buffer, key, transform_offset);
        }
        return buffer;
    };

    std::vector<std::uint8_t> toc_bytes = read_block(header.toc_offset, header.toc_size);

    if (static_cast<Compressor>(header.toc_compressor) == Compressor::Zlib) {
        toc_bytes = zlib_decompress(toc_bytes, static_cast<std::size_t>(header.number_of_files * sizeof(TocEntry)));
    }
    if (toc_bytes.size() != header.number_of_files * sizeof(TocEntry)) {
        throw TreArchiveError("TOC block has unexpected size");
    }

    std::vector<TocEntry> toc(header.number_of_files);
    std::memcpy(toc.data(), toc_bytes.data(), toc_bytes.size());

    const std::uint32_t name_block_offset = header.toc_offset + header.toc_size;
    std::vector<std::uint8_t> name_block = read_block(name_block_offset, header.name_block_size);
    if (static_cast<Compressor>(header.name_block_compressor) == Compressor::Zlib) {
        name_block = zlib_decompress(name_block, header.name_block_uncompressed_size);
    }
    if (name_block.size() != header.name_block_uncompressed_size) {
        throw TreArchiveError("Name block has unexpected size");
    }

    TreArchive archive;
    for (const TocEntry &entry : toc) {
        if (entry.file_name_offset >= name_block.size()) {
            throw TreArchiveError("File name offset out of bounds");
        }
        const char *start = reinterpret_cast<const char *>(name_block.data()) + entry.file_name_offset;
        const char *end = static_cast<const char *>(memchr(start, '\0', name_block.size() - entry.file_name_offset));
        if (!end) {
            throw TreArchiveError("Unterminated file name in archive");
        }
        std::string name(start, end);

        const std::uint32_t encrypted_offset = entry.offset;
        std::vector<std::uint8_t> payload = read_block(
            encrypted_offset,
            entry.compressed_length ? entry.compressed_length : entry.length
        );

        std::vector<std::uint8_t> data;
        bool uncompressed = entry.compressor == static_cast<std::uint32_t>(Compressor::None);
        if (static_cast<Compressor>(entry.compressor) == Compressor::Zlib) {
            data = zlib_decompress(payload, entry.length);
        } else if (static_cast<Compressor>(entry.compressor) == Compressor::None) {
            data = std::move(payload);
            if (data.size() != entry.length) {
                throw TreArchiveError("Entry length mismatch");
            }
        } else {
            throw TreArchiveError("Encountered unsupported entry compressor");
        }

        archive.m_entries.push_back(Entry{std::move(name), std::move(data), uncompressed});
    }

    return archive;
}

void TreArchive::add_file(const std::string &disk_path, const std::string &archive_name) {
    if (archive_name.empty()) {
        throw TreArchiveError("Archive entry name cannot be empty");
    }
    add_bytes(archive_name, read_file_bytes(disk_path));
}

void TreArchive::add_bytes(const std::string &archive_name, std::vector<std::uint8_t> bytes, bool store_uncompressed) {
    if (archive_name.empty()) {
        throw TreArchiveError("Archive entry name cannot be empty");
    }
    Entry entry;
    entry.name = normalize_name(archive_name);
    entry.data = std::move(bytes);
    entry.uncompressed = store_uncompressed;
    m_entries.push_back(std::move(entry));
}

void TreArchive::remove_entry(std::size_t index) {
    if (index >= m_entries.size()) {
        return;
    }
    m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(index));
}

void TreArchive::save(const std::string &path, const std::string &passphrase) const {
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) {
        throw TreArchiveError("Unable to open archive for writing: " + path);
    }

    const bool encrypt = !passphrase.empty();
    std::array<std::uint8_t, 16> key{};
    if (encrypt) {
        key = derive_key(passphrase);
    }

    std::vector<Entry> sorted_entries = m_entries;
    std::sort(sorted_entries.begin(), sorted_entries.end(), [](const Entry &a, const Entry &b) {
        const std::uint32_t crc_a = crc_string(a.name);
        const std::uint32_t crc_b = crc_string(b.name);
        if (crc_a == crc_b) {
            return a.name < b.name;
        }
        return crc_a < crc_b;
    });

    std::vector<std::uint8_t> name_block;
    name_block.reserve(sorted_entries.size() * 32);
    std::vector<TocEntry> toc;
    toc.reserve(sorted_entries.size());

    std::uint32_t data_offset = sizeof(Header);
    data_offset += static_cast<std::uint32_t>(sorted_entries.size() * sizeof(TocEntry));

    for (const Entry &entry : sorted_entries) {
        const std::uint32_t name_offset = static_cast<std::uint32_t>(name_block.size());
        name_block.insert(name_block.end(), entry.name.begin(), entry.name.end());
        name_block.push_back('\0');

        toc.push_back(TocEntry{
            .crc = crc_string(entry.name),
            .length = static_cast<std::uint32_t>(entry.data.size()),
            .offset = 0, // patched below
            .compressor = entry.uncompressed ? static_cast<std::uint32_t>(Compressor::None) : static_cast<std::uint32_t>(Compressor::Zlib),
            .compressed_length = 0,
            .file_name_offset = name_offset,
        });
    }

    data_offset += static_cast<std::uint32_t>(name_block.size());

    std::vector<std::vector<std::uint8_t>> payloads;
    payloads.reserve(sorted_entries.size());

    for (std::size_t i = 0; i < sorted_entries.size(); ++i) {
        std::vector<std::uint8_t> payload;
        if (sorted_entries[i].uncompressed) {
            payload = sorted_entries[i].data;
            toc[i].compressed_length = toc[i].length;
        } else {
            payload = zlib_compress(sorted_entries[i].data);
            toc[i].compressed_length = static_cast<std::uint32_t>(payload.size());
        }
        toc[i].offset = data_offset;
        data_offset += toc[i].compressed_length;
        payloads.push_back(std::move(payload));
    }

    Header header{};
    header.token = encrypt ? TAG_TRES : TAG_TREE;
    header.version = TAG_0005;
    header.number_of_files = static_cast<std::uint32_t>(sorted_entries.size());
    header.toc_offset = sizeof(Header);
    header.toc_compressor = static_cast<std::uint32_t>(Compressor::None);
    header.toc_size = static_cast<std::uint32_t>(toc.size() * sizeof(TocEntry));
    header.name_block_compressor = static_cast<std::uint32_t>(Compressor::None);
    header.name_block_size = static_cast<std::uint32_t>(name_block.size());
    header.name_block_uncompressed_size = static_cast<std::uint32_t>(name_block.size());

    write_u32(out, header.token);
    write_u32(out, header.version);
    write_u32(out, header.number_of_files);
    write_u32(out, header.toc_offset);
    write_u32(out, header.toc_compressor);
    write_u32(out, header.toc_size);
    write_u32(out, header.name_block_compressor);
    write_u32(out, header.name_block_size);
    write_u32(out, header.name_block_uncompressed_size);

    std::vector<std::uint8_t> content;
    content.reserve(static_cast<std::size_t>(data_offset - static_cast<std::uint32_t>(sizeof(Header))));

    const std::size_t toc_byte_count = toc.size() * sizeof(TocEntry);
    const auto *toc_start = reinterpret_cast<const std::uint8_t *>(toc.data());
    content.insert(content.end(), toc_start, toc_start + toc_byte_count);

    const auto append_bytes = [&content](const std::vector<std::uint8_t> &container) {
        if (container.empty()) {
            return;
        }
        const std::size_t byte_count = container.size() * sizeof(std::uint8_t);
        const auto *start = reinterpret_cast<const std::uint8_t *>(container.data());
        content.insert(content.end(), start, start + byte_count);
    };

    append_bytes(name_block);
    for (const auto &payload : payloads) {
        append_bytes(payload);
    }

    if (encrypt) {
        transform_buffer(content, key, 0);
    }

    out.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size()));
    if (!out) {
        throw TreArchiveError("Failed while writing archive payload");
    }
}

std::string format_bytes(const std::vector<std::uint8_t> &data) {
    if (data.empty()) {
        return "(empty)";
    }
    std::ostringstream ss;
    ss.setf(std::ios::hex, std::ios::basefield);
    ss.fill('0');
    for (std::size_t i = 0; i < data.size(); ++i) {
        ss.width(2);
        ss << static_cast<int>(data[i]);
        if (i + 1 < data.size()) {
            ss << ' ';
        }
    }
    return ss.str();
}
