#pragma once
#include <functional>

#include <ftxui/component/component.hpp>

namespace szip::tui {

using OnBack = std::function<void()>;

using ResetFn = std::function<void()>;

ftxui::Component ListView(OnBack on_back, ResetFn* out_reset = nullptr);

}  // namespace szip::tui
