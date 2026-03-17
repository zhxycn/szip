#include "lzw.h"
#include <szip/errors.h>
#include "io/bitstream.h"
#include <cstring>

namespace sz {

LzwCodec::LzwCodec(uint8_t min_bits, uint8_t max_bits)
    : min_code_bits_(min_bits), max_code_bits_(max_bits) {
    if (min_bits < 9 || min_bits > max_bits || max_bits > 24) {
        throw CodecError("LZW: invalid code bit range");
    }
}

size_t LzwCodec::estimateMemoryUsage() const noexcept {
    // Each TrieNode is 256 * 4 + 4 ≈ 1028 bytes
    // Max dict size = 2^max_code_bits nodes
    return static_cast<size_t>(maxDictSize()) * sizeof(TrieNode);
}

void LzwCodec::buildCompressorState(const uint8_t* /*data*/, size_t /*size*/) {
    // LZW is a streaming algorithm — no pre-scan needed.
    // Trie is initialized fresh in encodeData.
}

void LzwCodec::serializeHeader(std::vector<uint8_t>& header) const {
    header = {min_code_bits_, max_code_bits_};
}

void LzwCodec::deserializeHeader(const uint8_t* header, uint32_t size) {
    if (size < 2) {
        throw CodecError("LZW: codec header too small");
    }
    min_code_bits_ = header[0];
    max_code_bits_ = header[1];
    if (min_code_bits_ < 9 || min_code_bits_ > max_code_bits_ || max_code_bits_ > 24) {
        throw CodecError("LZW: invalid code bit range in header");
    }
}

// ---------------------------------------------------------------------------
// Encode
// ---------------------------------------------------------------------------

void LzwCodec::initEncodeTrie() {
    trie_.clear();
    // Root node (index 0) — its children[c] point to single-byte entries
    trie_.emplace_back();

    for (int c = 0; c < 256; ++c) {
        int idx = static_cast<int>(trie_.size());
        TrieNode leaf;
        leaf.code = static_cast<uint32_t>(c);
        trie_.push_back(leaf);
        trie_[0].children[c] = static_cast<int32_t>(idx);
    }

    next_code_ = firstAvailableCode();
    current_bits_ = min_code_bits_;
}

void LzwCodec::encodeData(const uint8_t* data, size_t size, BitWriter& writer) {
    initEncodeTrie();

    writer.writeBits(clearCode(), current_bits_);

    if (size == 0) {
        writer.writeBits(eoiCode(), current_bits_);
        return;
    }

    int current_node = trie_[0].children[data[0]];

    for (size_t i = 1; i < size; ++i) {
        uint8_t c = data[i];
        int32_t next = trie_[current_node].children[c];

        if (next != -1) {
            current_node = next;
        } else {
            // Output current match
            writer.writeBits(trie_[current_node].code, current_bits_);

            // Add new entry if dictionary not full
            if (next_code_ < maxDictSize()) {
                int new_idx = static_cast<int>(trie_.size());
                TrieNode new_node;
                new_node.code = next_code_;
                trie_.push_back(new_node);
                trie_[current_node].children[c] = static_cast<int32_t>(new_idx);
                ++next_code_;

                // Grow code width if needed
                if (next_code_ > (1u << current_bits_)) {
                    ++current_bits_;
                }
            } else {
                // Dictionary full — emit CLEAR and reset
                writer.writeBits(clearCode(), current_bits_);
                initEncodeTrie();
            }

            current_node = trie_[0].children[c];
        }
    }

    // Output last match
    writer.writeBits(trie_[current_node].code, current_bits_);
    writer.writeBits(eoiCode(), current_bits_);
}

// ---------------------------------------------------------------------------
// Decode
// ---------------------------------------------------------------------------

void LzwCodec::initDecodeDictionary() {
    decode_dict_.clear();
    decode_dict_.reserve(firstAvailableCode());

    for (int c = 0; c < 256; ++c) {
        decode_dict_.push_back(std::string(1, static_cast<char>(c)));
    }
    // CLEAR_CODE (256) and EOI_CODE (257) — placeholders
    decode_dict_.emplace_back();  // CLEAR
    decode_dict_.emplace_back();  // EOI

    next_code_ = firstAvailableCode();
    current_bits_ = min_code_bits_;
}

void LzwCodec::decodeData(BitReader& reader, uint64_t /*bit_count*/,
                           uint8_t* output, size_t original_size) {
    initDecodeDictionary();

    size_t out_pos = 0;
    std::string prev;

    while (out_pos < original_size) {
        uint32_t code = static_cast<uint32_t>(reader.readBits(current_bits_));

        if (code == eoiCode()) {
            break;
        }

        if (code == clearCode()) {
            initDecodeDictionary();
            prev.clear();
            continue;
        }

        std::string entry;
        if (code < static_cast<uint32_t>(decode_dict_.size())) {
            entry = decode_dict_[code];
        } else if (code == next_code_ && !prev.empty()) {
            // Special case: code not yet in dictionary
            entry = prev + prev[0];
        } else {
            throw CodecError("LZW: invalid code during decompression");
        }

        // Write to output
        size_t to_write = std::min(entry.size(), original_size - out_pos);
        std::memcpy(output + out_pos, entry.data(), to_write);
        out_pos += to_write;

        // Add new dictionary entry
        if (!prev.empty() && next_code_ < maxDictSize()) {
            decode_dict_.push_back(prev + entry[0]);
            ++next_code_;
            // Decoder grows one step earlier (>=) than encoder (>) because
            // the decoder's dictionary lags by one entry.
            if (next_code_ >= (1u << current_bits_) && current_bits_ < max_code_bits_) {
                ++current_bits_;
            }
        }

        prev = std::move(entry);
    }
}

// ---------------------------------------------------------------------------
// resetState
// ---------------------------------------------------------------------------

void LzwCodec::resetState() {
    trie_.clear();
    decode_dict_.clear();
    next_code_ = 0;
    current_bits_ = 0;
}

}  // namespace sz
