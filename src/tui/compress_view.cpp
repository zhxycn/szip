#include "compress_view.h"
#include "file_picker.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <memory>

namespace szip::tui {

using namespace ftxui;

Component CompressView(OnBack on_back, OnStartCompress on_start,
                       ResetFn* out_reset) {
    struct State {
        FilePicker picker{{.multi_select = true}};
        int method_index = 0;
        int mode_index = 0;
        std::string memory_str = "256";
        std::string output_path = "output.sz";
        std::vector<std::string> methods = {"Huffman", "LZW"};
        std::vector<std::string> modes = {"Native (.sz)", "Tar (.tar.sz)"};
    };
    auto st = std::make_shared<State>();

    if (out_reset) {
        *out_reset = [st] {
            st->picker.clearSelection();
            st->method_index = 0;
            st->mode_index = 0;
            st->output_path = "output.sz";
        };
    }

    auto picker_comp = st->picker.component();

    auto method_radio = Radiobox(&st->methods, &st->method_index);
    auto mode_radio = Radiobox(&st->modes, &st->mode_index);

    auto memory_input = Input(&st->memory_str, "256");
    auto output_input = Input(&st->output_path, "output.sz");

    auto start_btn = Button("  Start Compress  ", [st, on_start] {
        auto selected = st->picker.selectedPaths();
        if (selected.empty()) return;

        std::vector<std::filesystem::path> parents;
        std::vector<std::filesystem::path> expanded_files;
        for (const auto& p : selected) {
            std::error_code ec;
            if (std::filesystem::is_directory(p, ec)) {
                parents.push_back(p);
                for (const auto& entry :
                     std::filesystem::recursive_directory_iterator(p, ec)) {
                    if (entry.is_regular_file(ec)) {
                        expanded_files.push_back(entry.path());
                    }
                }
            } else {
                parents.push_back(p.parent_path());
                expanded_files.push_back(p);
            }
        }

        if (expanded_files.empty()) return;

        std::filesystem::path base_dir;
        if (!parents.empty()) {
            base_dir = parents.front();
            for (size_t i = 1; i < parents.size(); ++i) {
                auto a = base_dir.string();
                auto b = parents[i].string();
                size_t len = std::min(a.size(), b.size());
                size_t common = 0;
                for (size_t j = 0; j < len; ++j) {
                    if (a[j] != b[j]) break;
                    common = j + 1;
                }
                base_dir = std::filesystem::path(a.substr(0, common));
            }
            if (!std::filesystem::is_directory(base_dir)) {
                base_dir = base_dir.parent_path();
            }
        }

        sz::CompressOptions opts;
        opts.method = (st->method_index == 1) ? sz::MethodId::LZW
                                               : sz::MethodId::Huffman;
        opts.mode = (st->mode_index == 1) ? sz::ArchiveMode::Tar
                                           : sz::ArchiveMode::Native;
        opts.base_dir = base_dir;
        try {
            opts.max_memory =
                static_cast<size_t>(std::stoi(st->memory_str)) * 1024 * 1024;
        } catch (...) {
            opts.max_memory = 256 * 1024 * 1024;
        }

        auto out = st->output_path;
        if (st->mode_index == 1) {
            if (out.find(".tar.sz") == std::string::npos &&
                out.find(".sz") != std::string::npos) {
                auto pos = out.rfind(".sz");
                out = out.substr(0, pos) + ".tar.sz";
            }
        }

        on_start(std::move(expanded_files), std::filesystem::path(out),
                 std::move(opts));
    });

    auto back_btn = Button("  Back to Menu    ", [on_back] { on_back(); });

    auto right_panel = Container::Vertical({
        method_radio,
        mode_radio,
        memory_input,
        output_input,
        start_btn,
        back_btn,
    });

    auto container = Container::Horizontal({picker_comp, right_panel});

    return Renderer(container, [st, picker_comp, right_panel, method_radio,
                                mode_radio, memory_input, output_input,
                                start_btn, back_btn] {
        auto selected = st->picker.selectedPaths();
        std::uintmax_t total_size = 0;
        for (const auto& p : selected) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(p, ec))
                total_size += std::filesystem::file_size(p, ec);
        }

        std::string sel_info = std::to_string(selected.size()) + " item(s) selected";
        if (!selected.empty()) {
            sel_info += " (" + FilePicker::formatSize(total_size) + ")";
        }

        bool can_start = !selected.empty();

        auto right = vbox({
            text(" Compress Options") | bold,
            separator(),
            text(""),
            text(" Algorithm:"),
            method_radio->Render(),
            text(""),
            text(" Archive Mode:"),
            mode_radio->Render(),
            text(""),
            text(" Max Memory (MB):"),
            memory_input->Render() | size(WIDTH, EQUAL, 15),
            text(""),
            text(" Output File:"),
            output_input->Render() | size(WIDTH, EQUAL, 30),
            text(""),
            separator(),
            text(" " + sel_info) | (can_start ? color(Color::Green) : dim),
            text(""),
            start_btn->Render() |
                (can_start ? nothing : dim),
            back_btn->Render(),
            filler(),
            text(" Tab:switch panel  Esc:back") | dim,
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
