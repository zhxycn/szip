#include "archive_writer.h"
#include <szip/errors.h>
#include "io/bitstream.h"
#include "io/crc32.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace sz {

ArchiveWriter::ArchiveWriter(MethodId method, size_t max_memory,
                             ProgressCallback on_progress,
                             std::filesystem::path base_dir)
    : method_(method), max_memory_(max_memory),
      on_progress_(std::move(on_progress)),
      base_dir_(std::move(base_dir)) {}

size_t ArchiveWriter::computeBlockSize(ICodec* codec) const {
    size_t overhead = codec->estimateMemoryUsage() + 64 * 1024;
    if (max_memory_ <= overhead * 2) {
        return 64 * 1024;  // minimum 64 KB block
    }
    return (max_memory_ - overhead) / 2;
}

void ArchiveWriter::compressBlock(ICodec* codec,
                                  const uint8_t* data, size_t size,
                                  std::ostream& os) {
    // Compress into a memory buffer to compare sizes
    std::ostringstream compressed_stream(std::ios::binary);
    BitWriter bw(compressed_stream);
    std::vector<uint8_t> codec_header;

    codec->compress(data, size, bw, codec_header);
    bw.flush();

    std::string compressed_data = compressed_stream.str();
    uint64_t compressed_bit_count = bw.bitsWritten();
    size_t compressed_byte_size = compressed_data.size();

    // Decide: use compressed or Store fallback?
    bool use_store = (compressed_byte_size + codec_header.size()) >= size;

    // Compute block CRC (over codec_header + compressed_data, or raw data for Store)
    Crc32 block_crc;
    BlockHeader bh;

    if (use_store) {
        bh.method_id = static_cast<uint8_t>(MethodId::Store);
        bh.original_size = size;
        bh.compressed_bit_count = static_cast<uint64_t>(size) * 8;
        bh.codec_header_size = 0;
        block_crc.update(data, size);
        bh.block_crc32 = block_crc.finalize();

        io::write(os, bh);
        io::writeBytes(os, data, size);
    } else {
        bh.method_id = static_cast<uint8_t>(codec->methodId());
        bh.original_size = size;
        bh.compressed_bit_count = compressed_bit_count;
        bh.codec_header_size = static_cast<uint32_t>(codec_header.size());

        block_crc.update(codec_header.data(), codec_header.size());
        block_crc.update(reinterpret_cast<const uint8_t*>(compressed_data.data()),
                         compressed_data.size());
        bh.block_crc32 = block_crc.finalize();

        io::write(os, bh);
        io::writeBytes(os, codec_header.data(), codec_header.size());
        io::writeBytes(os, compressed_data.data(), compressed_data.size());
    }
}

void ArchiveWriter::write(const std::vector<std::filesystem::path>& files,
                          const std::filesystem::path& output_path,
                          uint16_t flags) {
    ICodec* codec = CodecRegistry::instance().getCodec(method_);
    if (!codec) {
        throw CodecError("No codec registered for method " +
                         std::to_string(static_cast<int>(method_)));
    }

    size_t block_size = computeBlockSize(codec);

    std::ofstream ofs(output_path, std::ios::binary);
    if (!ofs) {
        throw IoError("Cannot create output file: " + output_path.string());
    }

    // 1. Write SignatureHeader
    SignatureHeader sig;
    sig.magic = SZ_MAGIC;
    sig.version_major = SZ_VERSION_MAJOR;
    sig.version_minor = SZ_VERSION_MINOR;
    sig.flags = flags;
    io::write(ofs, sig);

    // Calculate total bytes for progress
    uint64_t total_bytes = 0;
    for (const auto& f : files) {
        total_bytes += std::filesystem::file_size(f);
    }
    uint64_t bytes_done = 0;

    // 2. Compress each file
    std::vector<CatalogEntry> catalog;
    std::vector<uint8_t> read_buf(block_size);

    for (const auto& filepath : files) {
        uint64_t file_size = std::filesystem::file_size(filepath);
        auto mtime = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::filesystem::last_write_time(filepath).time_since_epoch()
            ).count());

        std::ifstream ifs(filepath, std::ios::binary);
        if (!ifs) {
            throw IoError("Cannot open file: " + filepath.string());
        }

        CatalogEntry entry;
        if (!base_dir_.empty()) {
            entry.filename = std::filesystem::relative(filepath, base_dir_).generic_string();
        } else {
            entry.filename = filepath.filename().string();
        }
        entry.original_size = file_size;
        entry.first_block_offset = static_cast<uint64_t>(ofs.tellp());
        entry.mtime = mtime;

        // Compute file CRC and block count
        Crc32 file_crc;
        uint32_t block_count = 0;
        uint64_t remaining = file_size;

        while (remaining > 0) {
            size_t chunk = static_cast<size_t>(
                std::min(remaining, static_cast<uint64_t>(block_size)));

            ifs.read(reinterpret_cast<char*>(read_buf.data()),
                     static_cast<std::streamsize>(chunk));

            file_crc.update(read_buf.data(), chunk);
            compressBlock(codec, read_buf.data(), chunk, ofs);

            ++block_count;
            remaining -= chunk;
            bytes_done += chunk;

            if (on_progress_) {
                on_progress_(bytes_done, total_bytes);
            }
        }

        entry.block_count = block_count;
        entry.crc32 = file_crc.finalize();
        catalog.push_back(std::move(entry));
    }

    // 3. Write Catalog
    uint64_t catalog_offset = static_cast<uint64_t>(ofs.tellp());
    std::ostringstream catalog_stream(std::ios::binary);
    for (const auto& entry : catalog) {
        io::write(catalog_stream, entry);
    }
    std::string catalog_bytes = catalog_stream.str();

    Crc32 catalog_crc;
    catalog_crc.update(reinterpret_cast<const uint8_t*>(catalog_bytes.data()),
                       catalog_bytes.size());

    io::writeBytes(ofs, catalog_bytes.data(), catalog_bytes.size());

    // 4. Write EndHeader
    EndHeader eh;
    eh.catalog_offset = catalog_offset;
    eh.catalog_size = catalog_bytes.size();
    eh.catalog_crc32 = catalog_crc.finalize();
    io::write(ofs, eh);
}

}  // namespace sz
