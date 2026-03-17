#pragma once
#include <filesystem>
#include <functional>
#include <string>

#include <ftxui/component/component.hpp>

namespace szip::tui {

using OnStartExtract = std::function<void(
    std::filesystem::path archive,
    std::filesystem::path output_dir,
    size_t max_memory)>;

using OnBack = std::function<void()>;

using ResetFn = std::function<void()>;

ftxui::Component ExtractView(OnBack on_back, OnStartExtract on_start,
                             ResetFn* out_reset = nullptr);

}  // namespace szip::tui
