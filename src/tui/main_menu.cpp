#include "main_menu.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

namespace szip::tui {

using namespace ftxui;

Component MainMenu(OnMenuSelect on_select) {
    struct MenuState {
        std::vector<std::string> entries = {
            "  Compress Files        ",
            "  Extract Archive       ",
            "  List Archive Contents ",
            "  Exit                  ",
        };
        int selected = 0;
    };
    auto ms = std::make_shared<MenuState>();

    auto menu = Menu(&ms->entries, &ms->selected);

    auto renderer = Renderer(menu, [menu, ms] {
        Elements lines;
        lines.push_back(text(""));
        lines.push_back(text("    ___  ____  __  ____  ") | bold | color(Color::Cyan));
        lines.push_back(text("   / __)(_   )(  )(  _ \\ ") | bold | color(Color::Cyan));
        lines.push_back(text("   \\__ \\ / /_  )(  ) __/ ") | bold | color(Color::Cyan));
        lines.push_back(text("   (___/(____)(__)(__)    ") | bold | color(Color::Cyan));
        lines.push_back(text(""));
        lines.push_back(separator());
        lines.push_back(text(""));
        lines.push_back(menu->Render() | hcenter);
        lines.push_back(text(""));
        lines.push_back(separator());
        lines.push_back(text("  Use Arrow Keys to navigate, Enter to select") | dim);

        return vbox(std::move(lines)) | border | center;
    });

    return CatchEvent(renderer, [on_select, ms](Event event) {
        if (event == Event::Return) {
            switch (ms->selected) {
                case 0: on_select(MenuAction::Compress); return true;
                case 1: on_select(MenuAction::Extract); return true;
                case 2: on_select(MenuAction::List); return true;
                case 3: on_select(MenuAction::Exit); return true;
                default: break;
            }
        }
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            on_select(MenuAction::Exit);
            return true;
        }
        return false;
    });
}

}  // namespace szip::tui
