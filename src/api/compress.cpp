#include <szip/compress.h>
#include <szip/errors.h>
#include "archive/archive_writer.h"
#include "codec/huffman.h"
#include "codec/lzw.h"
#include "archive/stream_compressor.h"

namespace sz {

static void ensureCodecsRegistered() {
    auto& reg = CodecRegistry::instance();
    if (!reg.getCodec(MethodId::Huffman)) {
        reg.registerCodec<HuffmanCodec>();
    }
    if (!reg.getCodec(MethodId::LZW)) {
        reg.registerCodec<LzwCodec>();
    }
}

void compressFiles(
    const std::vector<std::filesystem::path>& input_files,
    const std::filesystem::path& output_path,
    const CompressOptions& opts)
{
    ensureCodecsRegistered();

    if (input_files.empty()) {
        throw SzipError("No input files specified");
    }

    if (opts.mode == ArchiveMode::Native) {
        ArchiveWriter aw(opts.method, opts.max_memory, opts.on_progress, opts.base_dir);
        aw.write(input_files, output_path);
    } else {
        StreamCompressor sc(opts.method, opts.max_memory, opts.on_progress, opts.base_dir);
        sc.compress(input_files, output_path);
    }
}

}  // namespace sz
