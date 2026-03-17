#include <szip/codec.h>
#include "io/bitstream.h"

namespace sz {

// ---------------------------------------------------------------------------
// CodecBase — Template Method implementations
// ---------------------------------------------------------------------------

void CodecBase::compress(
    const uint8_t* data, size_t size,
    BitWriter& writer,
    std::vector<uint8_t>& codec_header)
{
    buildCompressorState(data, size);
    serializeHeader(codec_header);
    encodeData(data, size, writer);
    resetState();
}

void CodecBase::decompress(
    BitReader& reader, uint64_t bit_count,
    const uint8_t* codec_header, uint32_t codec_header_size,
    uint8_t* output, size_t original_size)
{
    deserializeHeader(codec_header, codec_header_size);
    decodeData(reader, bit_count, output, original_size);
    resetState();
}

// ---------------------------------------------------------------------------
// CodecRegistry
// ---------------------------------------------------------------------------

CodecRegistry& CodecRegistry::instance() {
    static CodecRegistry reg;
    return reg;
}

ICodec* CodecRegistry::getCodec(MethodId id) const {
    auto it = codecs_.find(id);
    if (it == codecs_.end()) {
        return nullptr;
    }
    return it->second.get();
}

}  // namespace sz
