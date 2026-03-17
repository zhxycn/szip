#include "tar.h"
#include <szip/errors.h>
#include <cstring>
#include <fstream>

namespace sz {

// ---------------------------------------------------------------------------
// TarWriter
// ---------------------------------------------------------------------------

void TarWriter::addFile(const std::filesystem::path& filepath,
                        const std::string& archive_name) {
    files_.push_back({filepath, archive_name});
}

void TarWriter::finalize(std::ostream& output) {
    for (const auto& f : files_) {
        auto size = std::filesystem::file_size(f.path);
        auto mtime = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::filesystem::last_write_time(f.path).time_since_epoch()
            ).count());

        writeHeader(output, f.archive_name, size, mtime);

        // Write file data
        std::ifstream ifs(f.path, std::ios::binary);
        if (!ifs) {
            throw IoError("tar: cannot open file: " + f.path.string());
        }

        constexpr size_t BUF_SIZE = 8192;
        char buf[BUF_SIZE];
        uint64_t remaining = size;
        while (remaining > 0) {
            auto to_read = static_cast<std::streamsize>(
                std::min(remaining, static_cast<uint64_t>(BUF_SIZE)));
            ifs.read(buf, to_read);
            output.write(buf, to_read);
            remaining -= static_cast<uint64_t>(to_read);
        }

        writePadding(output, size);
    }

    // Two zero blocks mark end of archive
    char zero_block[512] = {};
    output.write(zero_block, 512);
    output.write(zero_block, 512);

    files_.clear();
}

void TarWriter::writeHeader(std::ostream& os, const std::string& name,
                            uint64_t size, uint64_t mtime) {
    char header[512] = {};

    // name (0-99)
    std::strncpy(header, name.c_str(), 99);

    // mode (100-107) — 0644
    std::snprintf(header + 100, 8, "%07o", 0644);

    // uid (108-115), gid (116-123)
    std::snprintf(header + 108, 8, "%07o", 0);
    std::snprintf(header + 116, 8, "%07o", 0);

    // size (124-135) — octal
    std::snprintf(header + 124, 12, "%011llo",
                  static_cast<unsigned long long>(size));

    // mtime (136-147) — octal
    std::snprintf(header + 136, 12, "%011llo",
                  static_cast<unsigned long long>(mtime));

    // typeflag (156) — '0' for regular file
    header[156] = '0';

    // magic (257-262) — "ustar"
    std::memcpy(header + 257, "ustar", 5);
    header[262] = ' ';

    // version (263-264)
    header[263] = ' ';
    header[264] = '\0';

    // Compute checksum: sum of all bytes with checksum field as spaces
    std::memset(header + 148, ' ', 8);
    uint32_t checksum = 0;
    for (int i = 0; i < 512; ++i) {
        checksum += static_cast<uint8_t>(header[i]);
    }
    std::snprintf(header + 148, 7, "%06o", checksum);
    header[155] = '\0';

    os.write(header, 512);
}

void TarWriter::writePadding(std::ostream& os, uint64_t data_size) {
    uint64_t remainder = data_size % 512;
    if (remainder > 0) {
        char pad[512] = {};
        os.write(pad, static_cast<std::streamsize>(512 - remainder));
    }
}

// ---------------------------------------------------------------------------
// TarReader
// ---------------------------------------------------------------------------

std::vector<TarReader::Entry> TarReader::parse(std::istream& input) {
    std::vector<Entry> entries;
    char block[512];

    while (true) {
        input.read(block, 512);
        if (!input || input.gcount() < 512) break;

        if (isZeroBlock(block)) {
            // Check for second zero block
            input.read(block, 512);
            break;
        }

        Entry entry;
        entry.name = std::string(block, strnlen(block, 100));
        entry.size = parseOctal(block + 124, 12);
        entry.mtime = parseOctal(block + 136, 12);

        // Read file data
        entry.data.resize(entry.size);
        if (entry.size > 0) {
            input.read(reinterpret_cast<char*>(entry.data.data()),
                       static_cast<std::streamsize>(entry.size));
        }

        // Skip padding
        uint64_t remainder = entry.size % 512;
        if (remainder > 0) {
            input.seekg(static_cast<std::streamoff>(512 - remainder),
                        std::ios::cur);
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}

uint64_t TarReader::parseOctal(const char* buf, size_t len) {
    uint64_t result = 0;
    for (size_t i = 0; i < len; ++i) {
        if (buf[i] >= '0' && buf[i] <= '7') {
            result = (result << 3) | static_cast<uint64_t>(buf[i] - '0');
        } else if (buf[i] == ' ' || buf[i] == '\0') {
            if (result > 0) break;
        }
    }
    return result;
}

bool TarReader::isZeroBlock(const char* block) {
    for (int i = 0; i < 512; ++i) {
        if (block[i] != '\0') return false;
    }
    return true;
}

}  // namespace sz
