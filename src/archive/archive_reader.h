#pragma once
#include <szip/format.h>
#include <filesystem>
#include <functional>
#include <vector>

namespace sz {

using ProgressCallback = std::function<void(uint64_t bytes_processed, uint64_t total_bytes)>;

class ArchiveReader {
public:
    explicit ArchiveReader(size_t max_memory = 256 * 1024 * 1024,
                           ProgressCallback on_progress = nullptr);

    void extract(const std::filesystem::path& archive_path,
                 const std::filesystem::path& output_dir);

    [[nodiscard]] std::vector<CatalogEntry> list(
        const std::filesystem::path& archive_path);

    [[nodiscard]] uint16_t readFlags(const std::filesystem::path& archive_path);

private:
    size_t max_memory_;
    ProgressCallback on_progress_;

    struct ArchiveMetadata {
        SignatureHeader sig;
        EndHeader end;
        std::vector<CatalogEntry> catalog;
    };

    ArchiveMetadata readMetadata(std::istream& is);
};

}  // namespace sz
