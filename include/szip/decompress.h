#pragma once
#include <szip/format.h>
#include <filesystem>
#include <functional>
#include <vector>

namespace sz {

using ProgressCallback = std::function<void(uint64_t bytes_processed, uint64_t total_bytes)>;

void decompressArchive(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& output_dir,
    size_t max_memory = 256 * 1024 * 1024,
    ProgressCallback on_progress = nullptr);

std::vector<CatalogEntry> listArchive(
    const std::filesystem::path& archive_path);

}  // namespace sz
