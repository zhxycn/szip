#include "stream_compressor.h"
#include <szip/errors.h>
#include <szip/format.h>
#include "archive_reader.h"
#include "archive_writer.h"
#include "tar/tar.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sz {

// ---------------------------------------------------------------------------
// StreamCompressor — tar the files, then compress as a single-entry .sz
// ---------------------------------------------------------------------------

StreamCompressor::StreamCompressor(MethodId method, size_t max_memory,
                                   ProgressCallback on_progress,
                                   std::filesystem::path base_dir)
    : method_(method), max_memory_(max_memory),
      on_progress_(std::move(on_progress)),
      base_dir_(std::move(base_dir)) {}

void StreamCompressor::compress(const std::vector<std::filesystem::path>& files,
                                const std::filesystem::path& output_path) {
    // 1. Create tar stream in a temp file
    auto temp_tar = output_path;
    temp_tar += ".tmp.tar";

    {
        std::ofstream tar_out(temp_tar, std::ios::binary);
        if (!tar_out) {
            throw IoError("Cannot create temp tar file: " + temp_tar.string());
        }

        TarWriter tw;
        for (const auto& f : files) {
            std::string archive_name;
            if (!base_dir_.empty()) {
                archive_name = std::filesystem::relative(f, base_dir_).generic_string();
            } else {
                archive_name = f.filename().string();
            }
            tw.addFile(f, archive_name);
        }
        tw.finalize(tar_out);
    }

    // 2. Use ArchiveWriter to compress the tar file as a single entry
    //    with FLAG_TAR_MODE set
    ArchiveWriter aw(method_, max_memory_, on_progress_);
    aw.write({temp_tar}, output_path, FLAG_TAR_MODE);

    // 3. Remove temp tar file
    std::filesystem::remove(temp_tar);
}

// ---------------------------------------------------------------------------
// StreamDecompressor — decompress .tar.sz, then extract tar
// ---------------------------------------------------------------------------

StreamDecompressor::StreamDecompressor(size_t max_memory,
                                       ProgressCallback on_progress)
    : max_memory_(max_memory), on_progress_(std::move(on_progress)) {}

void StreamDecompressor::decompress(const std::filesystem::path& archive_path,
                                    const std::filesystem::path& output_dir) {
    // 1. Extract to temp dir (produces the tar file)
    auto temp_dir = output_dir;
    temp_dir += ".tmp_extract";
    std::filesystem::create_directories(temp_dir);

    {
        ArchiveReader ar(max_memory_, on_progress_);
        ar.extract(archive_path, temp_dir);
    }

    // 2. Find the extracted tar file and parse it
    std::filesystem::create_directories(output_dir);

    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        std::ifstream tar_in(entry.path(), std::ios::binary);
        if (!tar_in) continue;

        auto tar_entries = TarReader::parse(tar_in);
        for (const auto& te : tar_entries) {
            auto out_path = output_dir / te.name;
            auto parent = out_path.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }

            std::ofstream ofs(out_path, std::ios::binary);
            if (!ofs) {
                throw IoError("Cannot create file: " + out_path.string());
            }
            ofs.write(reinterpret_cast<const char*>(te.data.data()),
                      static_cast<std::streamsize>(te.data.size()));
        }
    }

    // 3. Cleanup temp
    std::filesystem::remove_all(temp_dir);
}

}  // namespace sz
