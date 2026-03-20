#include "list_view.h"
#include "file_picker.h"
#include "utf8_utils.h"

#include <szip/decompress.h>
#include <szip/errors.h>
#include <szip/format.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <memory>
#include <string>
#include <vector>

namespace szip::tui {

using namespace ftxui;

Component ListView(OnBack on_back, ResetFn* out_reset) {
    struct State {
        FilePicker picker{{
            .multi_select = false,
            .extension_filter = {".sz"},
        }};
        std::string selected_archive;
        std::vector<sz::CatalogEntry> entries;
        std::string error;
        bool loaded = false;
    };
    auto st = std::make_shared<State>();

    if (out_reset) {
        *out_reset = [st] {
            st->picker.clearSelection();
            st->selected_archive.clear();
            st->entries.clear();
            st->error.clear();
            st->loaded = false;
        };
    }

    st->picker.onSubmit([st](const std::filesystem::path& p) {
        st->selected_archive = pathToUtf8(p);
        st->error.clear();
        st->entries.clear();
        st->loaded = false;
        try {
            st->entries = sz::listArchive(p);
            st->loaded = true;
        } catch (const std::exception& e) {
            st->error = e.what();
            st->loaded = true;
        }
    });

    auto picker_comp = st->picker.component();
    auto back_btn = Button("  Back to Menu  ", [on_back] { on_back(); });

    auto container = Container::Horizontal({picker_comp, back_btn});

    return Renderer(container, [st, picker_comp, back_btn] {
        Elements table_rows;

        if (!st->loaded && st->selected_archive.empty()) {
            table_rows.push_back(
                text("  Select a .sz archive to view its contents.") | dim);
        } else if (!st->error.empty()) {
            table_rows.push_back(
                text("  Error: " + st->error) | color(Color::Red));
        } else if (st->entries.empty()) {
            table_rows.push_back(text("  (empty archive)") | dim);
        } else {
            // Header
            table_rows.push_back(hbox({
                text("  Name") | bold | size(WIDTH, EQUAL, 25),
                text("Size") | bold | size(WIDTH, EQUAL, 14),
                text("Blocks") | bold | size(WIDTH, EQUAL, 8),
                text("CRC32") | bold | size(WIDTH, EQUAL, 12),
            }));
            table_rows.push_back(separator());

            for (const auto& e : st->entries) {
                char crc_buf[12];
                std::snprintf(crc_buf, sizeof(crc_buf), "%08X", e.crc32);

                auto display_fn = truncateToWidth(e.filename, 22);
                table_rows.push_back(hbox({
                    text("  " + display_fn) | size(WIDTH, EQUAL, 25),
                    text(FilePicker::formatSize(e.original_size)) |
                        dim | size(WIDTH, EQUAL, 14),
                    text(std::to_string(e.block_count)) |
                        dim | size(WIDTH, EQUAL, 8),
                    text(std::string(crc_buf)) |
                        dim | size(WIDTH, EQUAL, 12),
                }));
            }
        }

        auto right = vbox({
            text(" Archive Contents") | bold,
            separator(),
            text(""),
            hbox({
                text(" Archive: "),
                text(st->selected_archive.empty() ? "(none)"
                                                   : st->selected_archive) |
                    dim,
            }),
            text(" Files: " + std::to_string(st->entries.size())) | dim,
            text(""),
            separator(),
            vbox(std::move(table_rows)) | vscroll_indicator | yframe | flex,
            separator(),
            back_btn->Render(),
            text(" Enter:select  Esc:back") | dim,
        }) | border | flex;

        return hbox({
            picker_comp->Render() | size(WIDTH, GREATER_THAN, 40) | flex,
            right | flex,
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
