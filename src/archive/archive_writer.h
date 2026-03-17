#pragma once
#include <szip/codec.h>
#include <szip/format.h>
#include <filesystem>
#include <functional>
#include <vector>

namespace sz {

using ProgressCallback = std::function<void(uint64_t bytes_processed, uint64_t total_bytes)>;

class ArchiveWriter {
public:
    ArchiveWriter(MethodId method, size_t max_memory,
                  ProgressCallback on_progress = nullptr,
                  std::filesystem::path base_dir = {});

    void write(const std::vector<std::filesystem::path>& files,
               const std::filesystem::path& output_path,
               uint16_t flags = 0);

private:
    MethodId method_;
    size_t max_memory_;
    ProgressCallback on_progress_;
    std::filesystem::path base_dir_;

    [[nodiscard]] size_t computeBlockSize(ICodec* codec) const;

    void compressBlock(ICodec* codec,
                       const uint8_t* data, size_t size,
                       std::ostream& os);
};

}  // namespace sz
