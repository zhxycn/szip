#pragma once
#include <szip/codec.h>
#include <filesystem>
#include <functional>
#include <vector>

namespace sz {

enum class ArchiveMode { Native, Tar };

using ProgressCallback = std::function<void(uint64_t bytes_processed, uint64_t total_bytes)>;

struct CompressOptions {
    MethodId method         = MethodId::Huffman;
    ArchiveMode mode        = ArchiveMode::Native;
    size_t max_memory       = 256 * 1024 * 1024;
    ProgressCallback on_progress = nullptr;
    std::filesystem::path base_dir;
};

void compressFiles(
    const std::vector<std::filesystem::path>& input_files,
    const std::filesystem::path& output_path,
    const CompressOptions& opts = {});

}  // namespace sz
