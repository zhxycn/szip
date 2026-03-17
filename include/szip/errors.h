#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace sz {

class SzipError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class FormatError : public SzipError {
    using SzipError::SzipError;
};

class ChecksumError : public SzipError {
public:
    ChecksumError(const std::string& context, uint32_t expected, uint32_t actual)
        : SzipError(context + ": expected CRC32 0x" + toHex(expected) +
                    ", got 0x" + toHex(actual)),
          expected_(expected),
          actual_(actual) {}

    [[nodiscard]] uint32_t expected() const noexcept { return expected_; }
    [[nodiscard]] uint32_t actual() const noexcept { return actual_; }

private:
    uint32_t expected_;
    uint32_t actual_;

    static std::string toHex(uint32_t v) {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%08X", v);
        return buf;
    }
};

class CodecError : public SzipError {
    using SzipError::SzipError;
};

class IoError : public SzipError {
    using SzipError::SzipError;
};

}  // namespace sz
