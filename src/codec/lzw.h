#pragma once
#include <szip/codec.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sz {

struct TrieNode {
    std::array<int32_t, 256> children;
    uint32_t code = 0;

    TrieNode() { children.fill(-1); }
};

class LzwCodec final : public CodecBase {
public:
    explicit LzwCodec(uint8_t min_bits = 9, uint8_t max_bits = 16);

    [[nodiscard]] MethodId methodId() const noexcept override { return MethodId::LZW; }
    [[nodiscard]] std::string_view name() const noexcept override { return "LZW"; }
    [[nodiscard]] size_t estimateMemoryUsage() const noexcept override;

protected:
    void buildCompressorState(const uint8_t* data, size_t size) override;
    void serializeHeader(std::vector<uint8_t>& header) const override;
    void deserializeHeader(const uint8_t* header, uint32_t size) override;
    void encodeData(const uint8_t* data, size_t size, BitWriter& writer) override;
    void decodeData(BitReader& reader, uint64_t bit_count,
                    uint8_t* output, size_t original_size) override;
    void resetState() override;

private:
    uint8_t min_code_bits_;
    uint8_t max_code_bits_;

    // Encode: Trie-based dictionary
    std::vector<TrieNode> trie_;
    // Decode: code → byte sequence
    std::vector<std::string> decode_dict_;

    uint32_t next_code_ = 0;
    uint8_t  current_bits_ = 0;

    static constexpr uint32_t CLEAR_CODE_OFFSET = 256;
    static constexpr uint32_t EOI_CODE_OFFSET   = 257;

    [[nodiscard]] uint32_t clearCode() const noexcept { return CLEAR_CODE_OFFSET; }
    [[nodiscard]] uint32_t eoiCode() const noexcept { return EOI_CODE_OFFSET; }
    [[nodiscard]] uint32_t firstAvailableCode() const noexcept { return EOI_CODE_OFFSET + 1; }
    [[nodiscard]] uint32_t maxDictSize() const noexcept { return 1u << max_code_bits_; }

    void initEncodeTrie();
    void initDecodeDictionary();
};

}  // namespace sz
