#pragma once
#include <szip/codec.h>
#include <filesystem>
#include <functional>
#include <vector>

namespace sz {

using ProgressCallback = std::function<void(uint64_t bytes_processed, uint64_t total_bytes)>;

// ---------------------------------------------------------------------------
// StreamCompressor — tar + compress for .tar.sz mode
// ---------------------------------------------------------------------------
class StreamCompressor {
public:
    StreamCompressor(MethodId method, size_t max_memory,
                     ProgressCallback on_progress = nullptr,
                     std::filesystem::path base_dir = {});

    void compress(const std::vector<std::filesystem::path>& files,
                  const std::filesystem::path& output_path);

private:
    MethodId method_;
    size_t max_memory_;
    ProgressCallback on_progress_;
    std::filesystem::path base_dir_;
};

// ---------------------------------------------------------------------------
// StreamDecompressor — decompress + untar for .tar.sz mode
// ---------------------------------------------------------------------------
class StreamDecompressor {
public:
    explicit StreamDecompressor(size_t max_memory = 256 * 1024 * 1024,
                                ProgressCallback on_progress = nullptr);

    void decompress(const std::filesystem::path& archive_path,
                    const std::filesystem::path& output_dir);

private:
    size_t max_memory_;
    ProgressCallback on_progress_;
};

}  // namespace sz
