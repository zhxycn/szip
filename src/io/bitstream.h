#pragma once
#include <cstdint>
#include <iostream>

namespace sz {

class BitWriter {
public:
    explicit BitWriter(std::ostream& os);
    ~BitWriter();

    BitWriter(const BitWriter&) = delete;
    BitWriter& operator=(const BitWriter&) = delete;

    void writeBit(bool bit);
    void writeBits(uint64_t value, int count);
    void flush();
    [[nodiscard]] uint64_t bitsWritten() const noexcept;

private:
    std::ostream& stream_;
    uint64_t accumulator_ = 0;
    int bits_used_ = 0;
    uint64_t total_bits_ = 0;

    void drainFullBytes();
};

class BitReader {
public:
    explicit BitReader(std::istream& is);

    BitReader(const BitReader&) = delete;
    BitReader& operator=(const BitReader&) = delete;

    [[nodiscard]] bool readBit();
    [[nodiscard]] uint64_t readBits(int count);
    [[nodiscard]] uint64_t bitsRead() const noexcept;

private:
    std::istream& stream_;
    uint64_t accumulator_ = 0;
    int bits_available_ = 0;
    uint64_t total_bits_ = 0;

    void refill();
};

}  // namespace sz
