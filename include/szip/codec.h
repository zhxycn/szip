#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sz {

class BitWriter;
class BitReader;

// ---------------------------------------------------------------------------
// MethodId — identifies a compression algorithm
// ---------------------------------------------------------------------------
enum class MethodId : uint8_t {
    Store   = 0x00,
    Huffman = 0x01,
    LZW     = 0x02,
};

// ---------------------------------------------------------------------------
// ICodec — pure virtual interface for compression codecs
// ---------------------------------------------------------------------------
class ICodec {
public:
    virtual ~ICodec() = default;

    [[nodiscard]] virtual MethodId methodId() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual size_t estimateMemoryUsage() const noexcept = 0;

    virtual void compress(
        const uint8_t* data, size_t size,
        BitWriter& writer,
        std::vector<uint8_t>& codec_header) = 0;

    virtual void decompress(
        BitReader& reader, uint64_t bit_count,
        const uint8_t* codec_header, uint32_t codec_header_size,
        uint8_t* output, size_t original_size) = 0;
};

// ---------------------------------------------------------------------------
// CodecBase — Template Method base class
//
// Defines the invariant skeleton:
//   compress:   buildCompressorState → serializeHeader → encodeData → resetState
//   decompress: deserializeHeader → decodeData → resetState
//
// Subclasses implement the algorithm-specific protected hooks.
// ---------------------------------------------------------------------------
class CodecBase : public ICodec {
public:
    void compress(
        const uint8_t* data, size_t size,
        BitWriter& writer,
        std::vector<uint8_t>& codec_header) final;

    void decompress(
        BitReader& reader, uint64_t bit_count,
        const uint8_t* codec_header, uint32_t codec_header_size,
        uint8_t* output, size_t original_size) final;

protected:
    virtual void buildCompressorState(const uint8_t* data, size_t size) = 0;
    virtual void serializeHeader(std::vector<uint8_t>& header) const = 0;
    virtual void deserializeHeader(const uint8_t* header, uint32_t size) = 0;
    virtual void encodeData(const uint8_t* data, size_t size, BitWriter& writer) = 0;
    virtual void decodeData(BitReader& reader, uint64_t bit_count,
                            uint8_t* output, size_t original_size) = 0;
    virtual void resetState() = 0;
};

// ---------------------------------------------------------------------------
// CodecRegistry — singleton registry of available codecs
// ---------------------------------------------------------------------------
class CodecRegistry {
public:
    static CodecRegistry& instance();

    template <typename T, typename... Args>
    void registerCodec(Args&&... args) {
        auto codec = std::make_unique<T>(std::forward<Args>(args)...);
        auto id = codec->methodId();
        codecs_.emplace(id, std::move(codec));
    }

    [[nodiscard]] ICodec* getCodec(MethodId id) const;

private:
    CodecRegistry() = default;
    std::unordered_map<MethodId, std::unique_ptr<ICodec>> codecs_;
};

}  // namespace sz
