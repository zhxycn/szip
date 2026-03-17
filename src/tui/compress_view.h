#pragma once
#include <szip/compress.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace szip::tui {

using OnStartCompress = std::function<void(
    std::vector<std::filesystem::path> files,
    std::filesystem::path output,
    sz::CompressOptions opts)>;

using OnBack = std::function<void()>;

using ResetFn = std::function<void()>;

ftxui::Component CompressView(OnBack on_back, OnStartCompress on_start,
                              ResetFn* out_reset = nullptr);

}  // namespace szip::tui
