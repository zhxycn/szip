#pragma once
#include <szip/compress.h>
#include <szip/decompress.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace szip::tui {

using OnDone = std::function<void()>;

struct ProgressState {
    std::mutex mtx;
    std::atomic<float> progress{0.0f};
    std::string status;
    std::string title;
    std::string detail1;
    std::string detail2;
    std::string detail3;
    bool done = false;
    std::string error;
};

ftxui::Component ProgressView(
    std::shared_ptr<ProgressState> state,
    OnDone on_back);

void runCompress(
    std::shared_ptr<ProgressState> state,
    ftxui::ScreenInteractive& screen,
    std::vector<std::filesystem::path> files,
    std::filesystem::path output,
    sz::CompressOptions opts);

void runExtract(
    std::shared_ptr<ProgressState> state,
    ftxui::ScreenInteractive& screen,
    std::filesystem::path archive,
    std::filesystem::path output_dir,
    size_t max_memory);

}  // namespace szip::tui
