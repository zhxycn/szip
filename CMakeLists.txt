cmake_minimum_required(VERSION 3.16)
project(szip VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# FTXUI
add_subdirectory(external/ftxui)

# szip core library
add_library(szip_core STATIC
    src/io/bitstream.cpp
    src/io/crc32.cpp
    src/codec/codec.cpp
    src/codec/huffman.cpp
    src/codec/lzw.cpp
    src/tar/tar.cpp
    src/archive/archive_writer.cpp
    src/archive/archive_reader.cpp
    src/archive/stream_compressor.cpp
    src/api/compress.cpp
    src/api/decompress.cpp
)

target_include_directories(szip_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(szip_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# szip executable
add_executable(szip
    src/main.cpp
    src/tui/app.cpp
    src/tui/file_picker.cpp
    src/tui/main_menu.cpp
    src/tui/compress_view.cpp
    src/tui/extract_view.cpp
    src/tui/list_view.cpp
    src/tui/progress_view.cpp
)

target_include_directories(szip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(szip PRIVATE
    szip_core
    ftxui::screen
    ftxui::dom
    ftxui::component
)
