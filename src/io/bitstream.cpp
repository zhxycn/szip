#include "bitstream.h"
#include <algorithm>
#include <stdexcept>

namespace sz {

// ---------------------------------------------------------------------------
// BitWriter
// ---------------------------------------------------------------------------

BitWriter::BitWriter(std::ostream& os) : stream_(os) {}

BitWriter::~BitWriter() {
    try {
        flush();
    } catch (...) {
    }
}

void BitWriter::writeBit(bool bit) {
    accumulator_ |= (static_cast<uint64_t>(bit ? 1 : 0) << bits_used_);
    ++bits_used_;
    ++total_bits_;
    if (bits_used_ >= 8) {
        drainFullBytes();
    }
}

void BitWriter::writeBits(uint64_t value, int count) {
    if (count <= 0) return;

    if (count < 64) {
        value &= (1ULL << count) - 1;
    }

    accumulator_ |= (value << bits_used_);
    bits_used_ += count;
    total_bits_ += count;

    if (bits_used_ >= 8) {
        drainFullBytes();
    }
}

void BitWriter::flush() {
    if (bits_used_ > 0) {
        stream_.put(static_cast<char>(accumulator_ & 0xFF));
        accumulator_ = 0;
        bits_used_ = 0;
    }
}

uint64_t BitWriter::bitsWritten() const noexcept {
    return total_bits_;
}

void BitWriter::drainFullBytes() {
    while (bits_used_ >= 8) {
        stream_.put(static_cast<char>(accumulator_ & 0xFF));
        accumulator_ >>= 8;
        bits_used_ -= 8;
    }
}

// ---------------------------------------------------------------------------
// BitReader
// ---------------------------------------------------------------------------

BitReader::BitReader(std::istream& is) : stream_(is) {}

bool BitReader::readBit() {
    if (bits_available_ == 0) {
        refill();
    }
    bool bit = (accumulator_ & 1) != 0;
    accumulator_ >>= 1;
    --bits_available_;
    ++total_bits_;
    return bit;
}

uint64_t BitReader::readBits(int count) {
    if (count <= 0) return 0;

    while (bits_available_ < count) {
        refill();
    }

    uint64_t mask = (count < 64) ? ((1ULL << count) - 1) : ~0ULL;
    uint64_t result = accumulator_ & mask;
    accumulator_ >>= count;
    bits_available_ -= count;
    total_bits_ += count;
    return result;
}

uint64_t BitReader::bitsRead() const noexcept {
    return total_bits_;
}

void BitReader::refill() {
    while (bits_available_ <= 56) {
        int ch = stream_.get();
        if (ch == std::istream::traits_type::eof()) {
            break;
        }
        accumulator_ |= (static_cast<uint64_t>(static_cast<uint8_t>(ch)) << bits_available_);
        bits_available_ += 8;
    }
}

}  // namespace sz
