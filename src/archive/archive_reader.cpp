#include "archive_reader.h"
#include <szip/codec.h>
#include <szip/errors.h>
#include "io/bitstream.h"
#include "io/crc32.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace sz {

ArchiveReader::ArchiveReader(size_t max_memory, ProgressCallback on_progress)
    : max_memory_(max_memory), on_progress_(std::move(on_progress)) {}

ArchiveReader::ArchiveMetadata ArchiveReader::readMetadata(std::istream& is) {
    ArchiveMetadata meta;

    // Read EndHeader from last 20 bytes
    is.seekg(-20, std::ios::end);
    meta.end = io::readEndHeader(is);

    // Validate Catalog CRC
    is.seekg(static_cast<std::streamoff>(meta.end.catalog_offset));
    std::vector<uint8_t> catalog_raw(meta.end.catalog_size);
    io::readBytes(is, catalog_raw.data(), meta.end.catalog_size);

    uint32_t actual_crc = crc32(catalog_raw.data(), catalog_raw.size());
    if (actual_crc != meta.end.catalog_crc32) {
        throw ChecksumError("Catalog", meta.end.catalog_crc32, actual_crc);
    }

    // Parse catalog entries
    std::istringstream catalog_stream(
        std::string(catalog_raw.begin(), catalog_raw.end()), std::ios::binary);
    while (catalog_stream.tellg() < static_cast<std::streampos>(meta.end.catalog_size)) {
        meta.catalog.push_back(io::readCatalogEntry(catalog_stream));
        if (!catalog_stream) break;
    }

    // Read and validate SignatureHeader
    is.seekg(0);
    meta.sig = io::readSignatureHeader(is);

    if (meta.sig.magic != SZ_MAGIC) {
        throw FormatError("Invalid magic number");
    }

    return meta;
}

std::vector<CatalogEntry> ArchiveReader::list(
    const std::filesystem::path& archive_path) {
    std::ifstream ifs(archive_path, std::ios::binary);
    if (!ifs) {
        throw IoError("Cannot open archive: " + archive_path.string());
    }
    auto meta = readMetadata(ifs);
    return meta.catalog;
}

uint16_t ArchiveReader::readFlags(const std::filesystem::path& archive_path) {
    std::ifstream ifs(archive_path, std::ios::binary);
    if (!ifs) {
        throw IoError("Cannot open archive: " + archive_path.string());
    }
    auto meta = readMetadata(ifs);
    return meta.sig.flags;
}

void ArchiveReader::extract(const std::filesystem::path& archive_path,
                            const std::filesystem::path& output_dir) {
    std::ifstream ifs(archive_path, std::ios::binary);
    if (!ifs) {
        throw IoError("Cannot open archive: " + archive_path.string());
    }

    auto meta = readMetadata(ifs);

    // Calculate total bytes for progress
    uint64_t total_bytes = 0;
    for (const auto& entry : meta.catalog) {
        total_bytes += entry.original_size;
    }
    uint64_t bytes_done = 0;

    std::filesystem::create_directories(output_dir);

    for (const auto& entry : meta.catalog) {
        auto out_path = output_dir / entry.filename;

        // Create parent directories if needed
        auto parent = out_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream ofs(out_path, std::ios::binary);
        if (!ofs) {
            throw IoError("Cannot create file: " + out_path.string());
        }

        ifs.seekg(static_cast<std::streamoff>(entry.first_block_offset));

        Crc32 file_crc;

        for (uint32_t b = 0; b < entry.block_count; ++b) {
            BlockHeader bh = io::readBlockHeader(ifs);

            // Read codec_header + compressed data as a blob for CRC check
            size_t payload_bytes = bh.codec_header_size +
                static_cast<size_t>((bh.compressed_bit_count + 7) / 8);
            std::vector<uint8_t> payload(payload_bytes);
            io::readBytes(ifs, payload.data(), payload_bytes);

            // Verify block CRC
            uint32_t actual_block_crc = crc32(payload.data(), payload_bytes);
            if (actual_block_crc != bh.block_crc32) {
                throw ChecksumError("Block " + std::to_string(b) + " of " + entry.filename,
                                    bh.block_crc32, actual_block_crc);
            }

            // Decompress
            std::vector<uint8_t> decompressed(bh.original_size);

            auto method = static_cast<MethodId>(bh.method_id);
            if (method == MethodId::Store) {
                if (bh.original_size > payload_bytes) {
                    throw FormatError("Store block: payload smaller than original_size");
                }
                std::memcpy(decompressed.data(), payload.data(), bh.original_size);
            } else {
                ICodec* codec = CodecRegistry::instance().getCodec(method);
                if (!codec) {
                    throw CodecError("Unknown method: " +
                                     std::to_string(bh.method_id));
                }

                const uint8_t* codec_header = payload.data();
                const uint8_t* compressed_data = payload.data() + bh.codec_header_size;
                size_t compressed_bytes = payload_bytes - bh.codec_header_size;

                std::istringstream bit_stream(
                    std::string(compressed_data, compressed_data + compressed_bytes),
                    std::ios::binary);
                BitReader br(bit_stream);

                codec->decompress(br, bh.compressed_bit_count,
                                  codec_header, bh.codec_header_size,
                                  decompressed.data(), bh.original_size);
            }

            ofs.write(reinterpret_cast<const char*>(decompressed.data()),
                      static_cast<std::streamsize>(bh.original_size));
            file_crc.update(decompressed.data(), bh.original_size);

            bytes_done += bh.original_size;
            if (on_progress_) {
                on_progress_(bytes_done, total_bytes);
            }
        }

        // Verify file CRC
        uint32_t actual_file_crc = file_crc.finalize();
        if (actual_file_crc != entry.crc32) {
            throw ChecksumError("File " + entry.filename, entry.crc32, actual_file_crc);
        }
    }
}

}  // namespace sz
