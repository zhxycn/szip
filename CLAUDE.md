# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Windows (MinGW) — default is Debug
build.bat [Release|Debug]

# Linux/macOS — default is Debug
./build.sh [Release|Debug]

# Manual CMake
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The FTXUI dependency is a git submodule under `external/ftxui`. Clone with `--recurse-submodules` or run `git submodule update --init --recursive`.

There are no tests in this project.

## Architecture

szip is a C++20 compression utility that produces `.sz` archives. It has both a CLI and an FTXUI-based terminal UI. Everything lives in the `sz` namespace.

### Layers

- **Public API** (`include/szip/`) — `compressFiles()` and `decompressArchive()`/`listArchive()` are the entry points. `CompressOptions` selects codec and archive mode.
- **Archive** (`src/archive/`) — `ArchiveWriter`/`ArchiveReader` handle the `.sz` container format: signature header, compressed data blocks, file catalog, and end header. `StreamCompressor` wraps block-level compression.
- **Codecs** (`src/codec/`) — Pluggable compression via `ICodec` interface and `CodecBase` (Template Method pattern). `CodecRegistry` singleton maps `MethodId` to codec instances. Currently: Huffman (0x01) and LZW (0x02).
- **I/O primitives** (`src/io/`) — `BitWriter`/`BitReader` for bit-level streaming, `CRC32` for checksums.
- **Tar** (`src/tar/`) — Optional POSIX ustar tar packing/unpacking, used when `ArchiveMode::Tar` is selected.
- **TUI** (`src/tui/`) — FTXUI views for interactive mode. `AppState` is mutex-protected shared state between the UI thread and worker thread.
- **CLI** (`src/main.cpp`) — Parses subcommands (`c`, `x`, `l`) or launches the TUI with no args.

### CMake targets

- `szip_core` — static library with all compression, archive, I/O, and tar code. Public headers in `include/`, private headers in `src/`.
- `szip` — executable linking `szip_core` + FTXUI (screen, dom, component).

### .sz file format

Defined in `include/szip/format.h`. All multi-byte fields are little-endian, serialized field-by-field (no `reinterpret_cast`). Layout: SignatureHeader (magic `SZIP`, version, flags) → BlockHeader + compressed data per block → Catalog (array of CatalogEntry) → EndHeader (catalog offset/size/crc).

### Adding a new codec

1. Create `src/codec/mycodec.h/.cpp` extending `CodecBase`.
2. Implement the six protected hooks: `buildCompressorState`, `serializeHeader`, `deserializeHeader`, `encodeData`, `decodeData`, `resetState`.
3. Add a `MethodId` enum value in `include/szip/codec.h`.
4. Register in `CodecRegistry` (see `src/codec/codec.cpp`).
5. Add source files to the `szip_core` target in `CMakeLists.txt`.
