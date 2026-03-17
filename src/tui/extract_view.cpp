#include "extract_view.h"
#include "file_picker.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <memory>

namespace szip::tui {

using namespace ftxui;

Component ExtractView(OnBack on_back, OnStartExtract on_start,
                      ResetFn* out_reset) {
    struct State {
        FilePicker picker{{
            .multi_select = false,
            .extension_filter = {".sz"},
        }};
        std::string output_dir = ".";
        std::string memory_str = "256";
        std::string selected_archive;
    };
    auto st = std::make_shared<State>();

    if (out_reset) {
        *out_reset = [st] {
            st->picker.clearSelection();
            st->selected_archive.clear();
            st->output_dir = ".";
        };
    }

    st->picker.onSubmit([st](const std::filesystem::path& p) {
        st->selected_archive = p.string();
    });

    auto picker_comp = st->picker.component();
    auto output_input = Input(&st->output_dir, "output directory");
    auto memory_input = Input(&st->memory_str, "256");

    auto start_btn = Button("  Start Extract  ", [st, on_start] {
        if (st->selected_archive.empty()) return;

        size_t max_mem = 256 * 1024 * 1024;
        try {
            max_mem = static_cast<size_t>(std::stoi(st->memory_str)) * 1024 * 1024;
        } catch (...) {}

        on_start(
            std::filesystem::path(st->selected_archive),
            std::filesystem::path(st->output_dir),
            max_mem);
    });

    auto back_btn = Button("  Back to Menu   ", [on_back] { on_back(); });

    auto right_panel = Container::Vertical({
        output_input,
        memory_input,
        start_btn,
        back_btn,
    });

    auto container = Container::Horizontal({picker_comp, right_panel});

    return Renderer(container, [st, picker_comp, right_panel, output_input,
                                memory_input, start_btn, back_btn] {
        bool can_start = !st->selected_archive.empty();

        auto right = vbox({
            text(" Extract Options") | bold,
            separator(),
            text(""),
            text(" Selected Archive:"),
            text(" " + (st->selected_archive.empty()
                            ? "(none - select a .sz file)"
                            : st->selected_archive)) |
                (can_start ? color(Color::Green) : dim),
            text(""),
            text(" Output Directory:"),
            output_input->Render() | size(WIDTH, EQUAL, 30),
            text(""),
            text(" Max Memory (MB):"),
            memory_input->Render() | size(WIDTH, EQUAL, 15),
            text(""),
            separator(),
            text(""),
            start_btn->Render() | (can_start ? nothing : dim),
            back_btn->Render(),
            filler(),
            text(" Enter:select file  Esc:back") | dim,
        }) | border | flex;

        return hbox({
            picker_comp->Render() | size(WIDTH, GREATER_THAN, 40) | flex,
            right | size(WIDTH, EQUAL, 35),
        });
    }) | CatchEvent([on_back](Event event) {
        if (event == Event::Escape) {
            on_back();
            return true;
        }
        return false;
    });
}

}  // namespace szip::tui
