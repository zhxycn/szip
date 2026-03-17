#pragma once
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace sz {

// ---------------------------------------------------------------------------
// TarWriter — pack files into a POSIX ustar tar stream
// ---------------------------------------------------------------------------
class TarWriter {
public:
    void addFile(const std::filesystem::path& filepath,
                 const std::string& archive_name);

    void finalize(std::ostream& output);

private:
    struct PendingFile {
        std::filesystem::path path;
        std::string archive_name;
    };
    std::vector<PendingFile> files_;

    static void writeHeader(std::ostream& os, const std::string& name,
                            uint64_t size, uint64_t mtime);
    static void writePadding(std::ostream& os, uint64_t data_size);
};

// ---------------------------------------------------------------------------
// TarReader — extract files from a POSIX ustar tar stream
// ---------------------------------------------------------------------------
class TarReader {
public:
    struct Entry {
        std::string name;
        uint64_t size = 0;
        uint64_t mtime = 0;
        std::vector<uint8_t> data;
    };

    static std::vector<Entry> parse(std::istream& input);

private:
    static uint64_t parseOctal(const char* buf, size_t len);
    static bool isZeroBlock(const char* block);
};

}  // namespace sz
