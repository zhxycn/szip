#include "progress_view.h"

#include <szip/errors.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <thread>

namespace szip::tui {

using namespace ftxui;

Component ProgressView(std::shared_ptr<ProgressState> state, OnDone on_back) {
    auto back_btn = Button("  Back to Menu  ", [state, on_back] {
        std::lock_guard lk(state->mtx);
        if (state->done) {
            on_back();
        }
    });

    auto container = Container::Vertical({back_btn});

    auto renderer = Renderer(container, [state, back_btn] {
        std::lock_guard lk(state->mtx);
        float p = state->progress.load();
        auto pct = std::to_string(static_cast<int>(p * 100)) + "%";

        Elements content;
        content.push_back(text("  " + state->title) | bold);
        content.push_back(separator());

        if (!state->detail1.empty())
            content.push_back(hbox({text("  "), text(state->detail1) | dim}));
        if (!state->detail2.empty())
            content.push_back(hbox({text("  "), text(state->detail2) | dim}));
        if (!state->detail3.empty())
            content.push_back(hbox({text("  "), text(state->detail3) | dim}));

        content.push_back(text(""));
        content.push_back(hbox({
            text("  ["),
            gauge(p) | flex,
            text("] " + pct),
        }));
        content.push_back(text(""));
        content.push_back(hbox({
            text("  Status: "),
            text(state->status),
        }));

        if (!state->error.empty()) {
            content.push_back(text("  Error: " + state->error) | color(Color::Red));
        }

        if (state->done) {
            content.push_back(text(""));
            content.push_back(back_btn->Render() | center);
            content.push_back(
                text("  (or press Esc / Q to return)") | dim | center);
        }

        return vbox(std::move(content)) | border | flex;
    });

    return CatchEvent(renderer, [state, on_back](Event event) {
        std::lock_guard lk(state->mtx);
        if (state->done &&
            (event == Event::Escape ||
             event == Event::Character('q') || event == Event::Character('Q'))) {
            on_back();
            return true;
        }
        return false;
    });
}

void runCompress(
    std::shared_ptr<ProgressState> state,
    ScreenInteractive& screen,
    std::vector<std::filesystem::path> files,
    std::filesystem::path output,
    sz::CompressOptions opts)
{
    {
        std::lock_guard lk(state->mtx);
        state->progress = 0.0f;
        state->done = false;
        state->error.clear();
        state->title = "szip - Compressing";
        state->detail1 = "Output: " + output.string();
        state->detail2 = std::string("Method: ") +
            (opts.method == sz::MethodId::LZW ? "LZW" : "Huffman");
        state->detail3 = std::string("Mode:   ") +
            (opts.mode == sz::ArchiveMode::Tar ? "Tar (.tar.sz)" : "Native (.sz)");
        state->status = "Compressing...";
    }

    opts.on_progress = [state, &screen](uint64_t done, uint64_t total) {
        if (total > 0)
            state->progress.store(
                static_cast<float>(done) / static_cast<float>(total));
        screen.PostEvent(Event::Custom);
    };

    std::thread([state, &screen, files = std::move(files),
                 output = std::move(output), opts = std::move(opts)] {
        try {
            sz::compressFiles(files, output, opts);
            std::lock_guard lk(state->mtx);
            state->status = "Done!";
        } catch (const std::exception& e) {
            std::lock_guard lk(state->mtx);
            state->error = e.what();
            state->status = "Error!";
        }
        {
            std::lock_guard lk(state->mtx);
            state->done = true;
        }
        screen.PostEvent(Event::Custom);
    }).detach();
}

void runExtract(
    std::shared_ptr<ProgressState> state,
    ScreenInteractive& screen,
    std::filesystem::path archive,
    std::filesystem::path output_dir,
    size_t max_memory)
{
    {
        std::lock_guard lk(state->mtx);
        state->progress = 0.0f;
        state->done = false;
        state->error.clear();
        state->title = "szip - Extracting";
        state->detail1 = "Archive: " + archive.string();
        state->detail2 = "Output:  " + output_dir.string();
        state->detail3.clear();
        state->status = "Extracting...";
    }

    auto on_progress = [state, &screen](uint64_t done, uint64_t total) {
        if (total > 0)
            state->progress.store(
                static_cast<float>(done) / static_cast<float>(total));
        screen.PostEvent(Event::Custom);
    };

    std::thread([state, &screen, archive = std::move(archive),
                 output_dir = std::move(output_dir), max_memory,
                 on_progress = std::move(on_progress)] {
        try {
            sz::decompressArchive(archive, output_dir, max_memory, on_progress);
            std::lock_guard lk(state->mtx);
            state->status = "Done!";
        } catch (const std::exception& e) {
            std::lock_guard lk(state->mtx);
            state->error = e.what();
            state->status = "Error!";
        }
        {
            std::lock_guard lk(state->mtx);
            state->done = true;
        }
        screen.PostEvent(Event::Custom);
    }).detach();
}

}  // namespace szip::tui
