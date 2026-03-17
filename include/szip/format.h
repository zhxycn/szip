#pragma once
#include <array>
#include <cstdint>
#include <iostream>

namespace sz {

// ---------------------------------------------------------------------------
// Magic & version constants
// ---------------------------------------------------------------------------
constexpr std::array<uint8_t, 4> SZ_MAGIC = {'S', 'Z', 'I', 'P'};
constexpr uint16_t SZ_VERSION_MAJOR = 1;
constexpr uint16_t SZ_VERSION_MINOR = 0;

// SignatureHeader.flags bit masks
constexpr uint16_t FLAG_TAR_MODE = 0x0001;

// ---------------------------------------------------------------------------
// On-disk structures (serialized field-by-field in little-endian, NOT via
// reinterpret_cast).  The structs here serve as in-memory representations.
// ---------------------------------------------------------------------------

struct SignatureHeader {
    std::array<uint8_t, 4> magic{};
    uint16_t version_major = 0;
    uint16_t version_minor = 0;
    uint16_t flags = 0;
    uint16_t reserved = 0;
};

struct BlockHeader {
    uint8_t  method_id = 0;
    uint64_t original_size = 0;
    uint64_t compressed_bit_count = 0;
    uint32_t codec_header_size = 0;
    uint32_t block_crc32 = 0;
};

struct CatalogEntry {
    std::string filename;
    uint64_t original_size = 0;
    uint64_t first_block_offset = 0;
    uint32_t block_count = 0;
    uint32_t crc32 = 0;
    uint64_t mtime = 0;
};

struct EndHeader {
    uint64_t catalog_offset = 0;
    uint64_t catalog_size = 0;
    uint32_t catalog_crc32 = 0;
};

// ---------------------------------------------------------------------------
// Little-endian I/O helpers
// ---------------------------------------------------------------------------
namespace io {

inline void writeLE16(std::ostream& os, uint16_t v) {
    const uint8_t buf[2] = {
        static_cast<uint8_t>(v),
        static_cast<uint8_t>(v >> 8),
    };
    os.write(reinterpret_cast<const char*>(buf), 2);
}

inline void writeLE32(std::ostream& os, uint32_t v) {
    const uint8_t buf[4] = {
        static_cast<uint8_t>(v),
        static_cast<uint8_t>(v >> 8),
        static_cast<uint8_t>(v >> 16),
        static_cast<uint8_t>(v >> 24),
    };
    os.write(reinterpret_cast<const char*>(buf), 4);
}

inline void writeLE64(std::ostream& os, uint64_t v) {
    const uint8_t buf[8] = {
        static_cast<uint8_t>(v),       static_cast<uint8_t>(v >> 8),
        static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24),
        static_cast<uint8_t>(v >> 32), static_cast<uint8_t>(v >> 40),
        static_cast<uint8_t>(v >> 48), static_cast<uint8_t>(v >> 56),
    };
    os.write(reinterpret_cast<const char*>(buf), 8);
}

inline uint16_t readLE16(std::istream& is) {
    uint8_t buf[2];
    is.read(reinterpret_cast<char*>(buf), 2);
    return static_cast<uint16_t>(buf[0]) |
           (static_cast<uint16_t>(buf[1]) << 8);
}

inline uint32_t readLE32(std::istream& is) {
    uint8_t buf[4];
    is.read(reinterpret_cast<char*>(buf), 4);
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

inline uint64_t readLE64(std::istream& is) {
    uint8_t buf[8];
    is.read(reinterpret_cast<char*>(buf), 8);
    return static_cast<uint64_t>(buf[0]) |
           (static_cast<uint64_t>(buf[1]) << 8) |
           (static_cast<uint64_t>(buf[2]) << 16) |
           (static_cast<uint64_t>(buf[3]) << 24) |
           (static_cast<uint64_t>(buf[4]) << 32) |
           (static_cast<uint64_t>(buf[5]) << 40) |
           (static_cast<uint64_t>(buf[6]) << 48) |
           (static_cast<uint64_t>(buf[7]) << 56);
}

inline void writeBytes(std::ostream& os, const void* data, size_t size) {
    os.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

inline void readBytes(std::istream& is, void* data, size_t size) {
    is.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
}

// ---------------------------------------------------------------------------
// Serialization of on-disk structures
// ---------------------------------------------------------------------------

inline void write(std::ostream& os, const SignatureHeader& h) {
    writeBytes(os, h.magic.data(), 4);
    writeLE16(os, h.version_major);
    writeLE16(os, h.version_minor);
    writeLE16(os, h.flags);
    writeLE16(os, h.reserved);
}

inline SignatureHeader readSignatureHeader(std::istream& is) {
    SignatureHeader h;
    readBytes(is, h.magic.data(), 4);
    h.version_major = readLE16(is);
    h.version_minor = readLE16(is);
    h.flags = readLE16(is);
    h.reserved = readLE16(is);
    return h;
}

inline void write(std::ostream& os, const BlockHeader& h) {
    os.put(static_cast<char>(h.method_id));
    writeLE64(os, h.original_size);
    writeLE64(os, h.compressed_bit_count);
    writeLE32(os, h.codec_header_size);
    writeLE32(os, h.block_crc32);
}

inline BlockHeader readBlockHeader(std::istream& is) {
    BlockHeader h;
    h.method_id = static_cast<uint8_t>(is.get());
    h.original_size = readLE64(is);
    h.compressed_bit_count = readLE64(is);
    h.codec_header_size = readLE32(is);
    h.block_crc32 = readLE32(is);
    return h;
}

inline void write(std::ostream& os, const CatalogEntry& e) {
    auto len = static_cast<uint16_t>(e.filename.size());
    writeLE16(os, len);
    writeBytes(os, e.filename.data(), len);
    writeLE64(os, e.original_size);
    writeLE64(os, e.first_block_offset);
    writeLE32(os, e.block_count);
    writeLE32(os, e.crc32);
    writeLE64(os, e.mtime);
}

inline CatalogEntry readCatalogEntry(std::istream& is) {
    CatalogEntry e;
    uint16_t len = readLE16(is);
    e.filename.resize(len);
    readBytes(is, e.filename.data(), len);
    e.original_size = readLE64(is);
    e.first_block_offset = readLE64(is);
    e.block_count = readLE32(is);
    e.crc32 = readLE32(is);
    e.mtime = readLE64(is);
    return e;
}

inline void write(std::ostream& os, const EndHeader& h) {
    writeLE64(os, h.catalog_offset);
    writeLE64(os, h.catalog_size);
    writeLE32(os, h.catalog_crc32);
}

inline EndHeader readEndHeader(std::istream& is) {
    EndHeader h;
    h.catalog_offset = readLE64(is);
    h.catalog_size = readLE64(is);
    h.catalog_crc32 = readLE32(is);
    return h;
}

}  // namespace io
}  // namespace sz
