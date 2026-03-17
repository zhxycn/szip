#include "app.h"
#include "compress_view.h"
#include "extract_view.h"
#include "list_view.h"
#include "main_menu.h"
#include "progress_view.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace szip::tui {

using namespace ftxui;

enum Screen { kMenu = 0, kCompress, kExtract, kList, kProgress };

int TuiApp::run() {
    auto screen = ScreenInteractive::Fullscreen();
    int current_screen = kMenu;
    auto progress_state = std::make_shared<ProgressState>();

    ResetFn reset_compress, reset_extract, reset_list;

    auto go = [&](int s) {
        current_screen = s;
    };

    auto menu = MainMenu([&](MenuAction action) {
        switch (action) {
            case MenuAction::Compress:
                if (reset_compress) reset_compress();
                go(kCompress);
                break;
            case MenuAction::Extract:
                if (reset_extract) reset_extract();
                go(kExtract);
                break;
            case MenuAction::List:
                if (reset_list) reset_list();
                go(kList);
                break;
            case MenuAction::Exit:
                screen.ExitLoopClosure()();
                break;
        }
    });

    auto compress = CompressView(
        [&]{ go(kMenu); },
        [&](std::vector<std::filesystem::path> files,
            std::filesystem::path output,
            sz::CompressOptions opts) {
            go(kProgress);
            runCompress(progress_state, screen,
                        std::move(files), std::move(output), std::move(opts));
        },
        &reset_compress);

    auto extract = ExtractView(
        [&]{ go(kMenu); },
        [&](std::filesystem::path archive,
            std::filesystem::path output_dir,
            size_t max_memory) {
            go(kProgress);
            runExtract(progress_state, screen,
                       std::move(archive), std::move(output_dir), max_memory);
        },
        &reset_extract);

    auto list = ListView([&]{ go(kMenu); }, &reset_list);

    auto progress = ProgressView(progress_state, [&]{ go(kMenu); });

    auto tab = Container::Tab(
        {menu, compress, extract, list, progress},
        &current_screen);

    screen.Loop(tab);
    return 0;
}

}  // namespace szip::tui
