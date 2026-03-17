#include "huffman.h"
#include <szip/errors.h>
#include "io/bitstream.h"
#include <algorithm>
#include <functional>
#include <queue>

namespace sz {

size_t HuffmanCodec::estimateMemoryUsage() const noexcept {
    // freq_table (2 KB) + code_table (~1.3 KB) + nodes (up to 511 * ~24 B ≈ 12 KB)
    return 64 * 1024;
}

// ---------------------------------------------------------------------------
// buildCompressorState — count byte frequencies, build tree, generate codes
// ---------------------------------------------------------------------------

void HuffmanCodec::buildCompressorState(const uint8_t* data, size_t size) {
    freq_table_.fill(0);
    for (size_t i = 0; i < size; ++i) {
        ++freq_table_[data[i]];
    }
    buildTree();
    if (root_ >= 0) {
        generateCodes(root_, 0, 0);
    }

    // Edge case: all bytes are the same value → single-node tree, assign 1-bit code
    if (root_ >= 0 && nodes_[root_].left == -1 && nodes_[root_].right == -1) {
        code_table_[nodes_[root_].data] = {0, 1};
    }
}

// ---------------------------------------------------------------------------
// serializeHeader — write non-zero frequency entries
// Format: uint16_t count, then count × (uint8_t byte, uint64_t freq)
// ---------------------------------------------------------------------------

void HuffmanCodec::serializeHeader(std::vector<uint8_t>& header) const {
    uint16_t count = 0;
    for (int i = 0; i < 256; ++i) {
        if (freq_table_[i] > 0) ++count;
    }

    header.clear();
    header.reserve(2 + count * 9);

    // count (LE16)
    header.push_back(static_cast<uint8_t>(count));
    header.push_back(static_cast<uint8_t>(count >> 8));

    for (int i = 0; i < 256; ++i) {
        if (freq_table_[i] > 0) {
            header.push_back(static_cast<uint8_t>(i));
            uint64_t f = freq_table_[i];
            for (int b = 0; b < 8; ++b) {
                header.push_back(static_cast<uint8_t>(f));
                f >>= 8;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// deserializeHeader — reconstruct frequency table and tree from header
// ---------------------------------------------------------------------------

void HuffmanCodec::deserializeHeader(const uint8_t* header, uint32_t size) {
    if (size < 2) {
        throw CodecError("Huffman: codec header too small");
    }

    uint16_t count = static_cast<uint16_t>(header[0]) |
                     (static_cast<uint16_t>(header[1]) << 8);

    if (size < 2u + count * 9u) {
        throw CodecError("Huffman: codec header truncated");
    }

    freq_table_.fill(0);
    const uint8_t* p = header + 2;

    for (uint16_t i = 0; i < count; ++i) {
        uint8_t byte_val = *p++;
        uint64_t freq = 0;
        for (int b = 0; b < 8; ++b) {
            freq |= static_cast<uint64_t>(*p++) << (b * 8);
        }
        freq_table_[byte_val] = freq;
    }

    buildTree();
    if (root_ >= 0) {
        generateCodes(root_, 0, 0);
    }
    if (root_ >= 0 && nodes_[root_].left == -1 && nodes_[root_].right == -1) {
        code_table_[nodes_[root_].data] = {0, 1};
    }
}

// ---------------------------------------------------------------------------
// encodeData — encode each byte using the code table
// ---------------------------------------------------------------------------

void HuffmanCodec::encodeData(const uint8_t* data, size_t size, BitWriter& writer) {
    for (size_t i = 0; i < size; ++i) {
        const auto& code = code_table_[data[i]];
        writer.writeBits(code.bits, code.length);
    }
}

// ---------------------------------------------------------------------------
// decodeData — walk the tree bit-by-bit to decode
// ---------------------------------------------------------------------------

void HuffmanCodec::decodeData(BitReader& reader, uint64_t /*bit_count*/,
                              uint8_t* output, size_t original_size) {
    if (root_ < 0) {
        throw CodecError("Huffman: tree not built");
    }

    // Single-symbol tree
    bool single_symbol = (nodes_[root_].left == -1 && nodes_[root_].right == -1);
    if (single_symbol) {
        uint8_t val = nodes_[root_].data;
        for (size_t i = 0; i < original_size; ++i) {
            (void)reader.readBit();
            output[i] = val;
        }
        return;
    }

    for (size_t i = 0; i < original_size; ++i) {
        int node = root_;
        while (nodes_[node].left != -1 || nodes_[node].right != -1) {
            bool bit = reader.readBit();
            node = bit ? nodes_[node].right : nodes_[node].left;
            if (node < 0) {
                throw CodecError("Huffman: invalid tree traversal");
            }
        }
        output[i] = nodes_[node].data;
    }
}

// ---------------------------------------------------------------------------
// resetState
// ---------------------------------------------------------------------------

void HuffmanCodec::resetState() {
    freq_table_.fill(0);
    code_table_.fill({});
    nodes_.clear();
    root_ = -1;
}

// ---------------------------------------------------------------------------
// buildTree — construct Huffman tree using a min-heap (priority queue)
// ---------------------------------------------------------------------------

void HuffmanCodec::buildTree() {
    nodes_.clear();
    root_ = -1;

    struct HeapEntry {
        uint64_t freq;
        int node_idx;
        bool operator>(const HeapEntry& o) const { return freq > o.freq; }
    };
    std::priority_queue<HeapEntry, std::vector<HeapEntry>, std::greater<>> heap;

    for (int i = 0; i < 256; ++i) {
        if (freq_table_[i] > 0) {
            int idx = static_cast<int>(nodes_.size());
            nodes_.push_back({static_cast<uint8_t>(i), freq_table_[i], -1, -1});
            heap.push({freq_table_[i], idx});
        }
    }

    if (heap.empty()) return;

    if (heap.size() == 1) {
        root_ = heap.top().node_idx;
        return;
    }

    while (heap.size() > 1) {
        auto a = heap.top(); heap.pop();
        auto b = heap.top(); heap.pop();

        int parent = static_cast<int>(nodes_.size());
        nodes_.push_back({0, a.freq + b.freq, a.node_idx, b.node_idx});
        heap.push({a.freq + b.freq, parent});
    }

    root_ = heap.top().node_idx;
}

// ---------------------------------------------------------------------------
// generateCodes — DFS to assign bit codes to each leaf
// ---------------------------------------------------------------------------

void HuffmanCodec::generateCodes(int node, uint32_t code, uint8_t depth) {
    if (node < 0) return;

    const auto& n = nodes_[node];
    if (n.left == -1 && n.right == -1) {
        code_table_[n.data] = {code, depth};
        return;
    }

    generateCodes(n.left, code, depth + 1);
    generateCodes(n.right, code | (1u << depth), depth + 1);
}

}  // namespace sz
