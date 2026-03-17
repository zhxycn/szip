#pragma once
#include <functional>

#include <ftxui/component/component.hpp>

namespace szip::tui {

enum class MenuAction { Compress, Extract, List, Exit };

using OnMenuSelect = std::function<void(MenuAction)>;

ftxui::Component MainMenu(OnMenuSelect on_select);

}  // namespace szip::tui
