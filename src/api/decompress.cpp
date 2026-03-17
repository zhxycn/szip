#include <szip/decompress.h>
#include <szip/codec.h>
#include <szip/errors.h>
#include <szip/format.h>
#include "archive/archive_reader.h"
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

void decompressArchive(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& output_dir,
    size_t max_memory,
    ProgressCallback on_progress)
{
    ensureCodecsRegistered();

    ArchiveReader ar_temp(max_memory);
    uint16_t flags = ar_temp.readFlags(archive_path);

    bool is_tar = (flags & FLAG_TAR_MODE) != 0;

    if (is_tar) {
        StreamDecompressor sd(max_memory, std::move(on_progress));
        sd.decompress(archive_path, output_dir);
    } else {
        ArchiveReader ar(max_memory, std::move(on_progress));
        ar.extract(archive_path, output_dir);
    }
}

std::vector<CatalogEntry> listArchive(
    const std::filesystem::path& archive_path)
{
    ensureCodecsRegistered();
    ArchiveReader ar;
    return ar.list(archive_path);
}

}  // namespace sz
