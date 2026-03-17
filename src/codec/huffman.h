#pragma once
#include <szip/codec.h>
#include <array>
#include <cstdint>
#include <vector>

namespace sz {

struct BitCode {
    uint32_t bits = 0;
    uint8_t  length = 0;
};

struct HuffmanNode {
    uint8_t  data = 0;
    uint64_t freq = 0;
    int left  = -1;
    int right = -1;
};

class HuffmanCodec final : public CodecBase {
public:
    [[nodiscard]] MethodId methodId() const noexcept override { return MethodId::Huffman; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Huffman"; }
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
    std::array<uint64_t, 256> freq_table_{};
    std::array<BitCode, 256>  code_table_{};
    std::vector<HuffmanNode>  nodes_;
    int root_ = -1;

    void buildTree();
    void generateCodes(int node, uint32_t code, uint8_t depth);
};

}  // namespace sz
