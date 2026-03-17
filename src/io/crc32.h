#pragma once
#include <cstddef>
#include <cstdint>

namespace sz {

class Crc32 {
public:
    void update(const uint8_t* data, size_t size) noexcept;
    void update(uint8_t byte) noexcept;
    [[nodiscard]] uint32_t finalize() const noexcept;
    void reset() noexcept;

private:
    uint32_t state_ = 0xFFFFFFFF;

    static const uint32_t table_[256];
};

inline uint32_t crc32(const uint8_t* data, size_t size) noexcept {
    Crc32 c;
    c.update(data, size);
    return c.finalize();
}

}  // namespace sz
