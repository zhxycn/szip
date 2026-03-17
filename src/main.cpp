#include <szip/compress.h>
#include <szip/decompress.h>
#include <szip/errors.h>

#include "tui/app.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

// Shared state for progress reporting between worker thread and UI
struct AppState {
    std::mutex mtx;
    std::atomic<float> progress{0.0f};
    std::string status;
    bool running = false;
    bool done = false;
    std::string error;
};

static void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " c [options] <output.sz> <file1> [file2 ...]\n"
              << "  " << prog << " x <archive.sz> [output_dir]\n"
              << "  " << prog << " l <archive.sz>\n"
              << "\nOptions:\n"
              << "  --method huffman|lzw   Compression method (default: huffman)\n"
              << "  --tar                  Use tar mode (.tar.sz)\n"
              << "  --memory <MB>          Max memory in MB (default: 256)\n";
}

static int cmdCompress(int argc, char* argv[]) {
    sz::CompressOptions opts;
    std::vector<fs::path> input_files;
    fs::path output_path;
    int positional = 0;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--method" && i + 1 < argc) {
            std::string m = argv[++i];
            if (m == "lzw") opts.method = sz::MethodId::LZW;
            else opts.method = sz::MethodId::Huffman;
        } else if (arg == "--tar") {
            opts.mode = sz::ArchiveMode::Tar;
        } else if (arg == "--memory" && i + 1 < argc) {
            opts.max_memory = static_cast<size_t>(std::stoi(argv[++i])) * 1024 * 1024;
        } else {
            if (positional == 0) {
                output_path = arg;
            } else {
                input_files.emplace_back(arg);
            }
            ++positional;
        }
    }

    if (output_path.empty() || input_files.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    // FTXUI progress UI
    auto screen = ScreenInteractive::Fullscreen();
    AppState state;

    opts.on_progress = [&](uint64_t done, uint64_t total) {
        if (total > 0) {
            state.progress.store(static_cast<float>(done) / static_cast<float>(total));
        }
        screen.PostEvent(Event::Custom);
    };

    std::thread worker([&] {
        {
            std::lock_guard lk(state.mtx);
            state.running = true;
            state.status = "Compressing...";
        }
        try {
            sz::compressFiles(input_files, output_path, opts);
            std::lock_guard lk(state.mtx);
            state.status = "Done!";
        } catch (const std::exception& e) {
            std::lock_guard lk(state.mtx);
            state.error = e.what();
            state.status = "Error!";
        }
        {
            std::lock_guard lk(state.mtx);
            state.done = true;
        }
        screen.PostEvent(Event::Custom);
    });

    auto renderer = Renderer([&] {
        std::lock_guard lk(state.mtx);
        float p = state.progress.load();
        auto pct = std::to_string(static_cast<int>(p * 100)) + "%";

        Elements content;
        content.push_back(text("  szip - Compressing") | bold);
        content.push_back(separator());

        content.push_back(hbox({
            text("  Output: "),
            text(output_path.string()) | dim,
        }));
        content.push_back(hbox({
            text("  Method: "),
            text(opts.method == sz::MethodId::LZW ? "LZW" : "Huffman") | dim,
        }));
        content.push_back(hbox({
            text("  Mode:   "),
            text(opts.mode == sz::ArchiveMode::Tar ? "tar" : "native") | dim,
        }));
        content.push_back(text(""));

        content.push_back(hbox({
            text("  ["),
            gauge(p) | flex,
            text("] " + pct),
        }));
        content.push_back(text(""));
        content.push_back(hbox({
            text("  Status: "),
            text(state.status),
        }));

        if (!state.error.empty()) {
            content.push_back(text("  Error: " + state.error) | color(Color::Red));
        }

        if (state.done) {
            content.push_back(text(""));
            content.push_back(text("  Press Q to exit.") | dim);
        }

        return vbox(std::move(content)) | border;
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (state.done && (event == Event::Character('q') ||
                           event == Event::Character('Q') ||
                           event == Event::Escape)) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    worker.join();

    return state.error.empty() ? 0 : 1;
}

static int cmdExtract(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    fs::path archive_path = argv[2];
    fs::path output_dir = (argc > 3) ? argv[3] : ".";

    auto screen = ScreenInteractive::Fullscreen();
    AppState state;

    sz::ProgressCallback on_progress = [&](uint64_t done, uint64_t total) {
        if (total > 0) {
            state.progress.store(static_cast<float>(done) / static_cast<float>(total));
        }
        screen.PostEvent(Event::Custom);
    };

    std::thread worker([&] {
        {
            std::lock_guard lk(state.mtx);
            state.running = true;
            state.status = "Extracting...";
        }
        try {
            sz::decompressArchive(archive_path, output_dir, 256 * 1024 * 1024,
                                  on_progress);
            std::lock_guard lk(state.mtx);
            state.status = "Done!";
        } catch (const std::exception& e) {
            std::lock_guard lk(state.mtx);
            state.error = e.what();
            state.status = "Error!";
        }
        {
            std::lock_guard lk(state.mtx);
            state.done = true;
        }
        screen.PostEvent(Event::Custom);
    });

    auto renderer = Renderer([&] {
        std::lock_guard lk(state.mtx);
        float p = state.progress.load();
        auto pct = std::to_string(static_cast<int>(p * 100)) + "%";

        Elements content;
        content.push_back(text("  szip - Extracting") | bold);
        content.push_back(separator());
        content.push_back(hbox({
            text("  Archive: "),
            text(archive_path.string()) | dim,
        }));
        content.push_back(hbox({
            text("  Output:  "),
            text(output_dir.string()) | dim,
        }));
        content.push_back(text(""));
        content.push_back(hbox({
            text("  ["),
            gauge(p) | flex,
            text("] " + pct),
        }));
        content.push_back(text(""));
        content.push_back(hbox({
            text("  Status: "),
            text(state.status),
        }));

        if (!state.error.empty()) {
            content.push_back(text("  Error: " + state.error) | color(Color::Red));
        }

        if (state.done) {
            content.push_back(text(""));
            content.push_back(text("  Press Q to exit.") | dim);
        }

        return vbox(std::move(content)) | border;
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (state.done && (event == Event::Character('q') ||
                           event == Event::Character('Q') ||
                           event == Event::Escape)) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    worker.join();

    return state.error.empty() ? 0 : 1;
}

static int cmdList(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    fs::path archive_path = argv[2];

    try {
        auto entries = sz::listArchive(archive_path);

        std::cout << "Archive: " << archive_path.string() << "\n";
        std::cout << "Files: " << entries.size() << "\n\n";

        std::cout << "  Size          Blocks  CRC32     Name\n";
        std::cout << "  ------------- ------- --------- ----------------\n";

        for (const auto& e : entries) {
            char line[256];
            std::snprintf(line, sizeof(line), "  %-13llu %-7u %08X  %s",
                          static_cast<unsigned long long>(e.original_size),
                          e.block_count,
                          e.crc32,
                          e.filename.c_str());
            std::cout << line << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        szip::tui::TuiApp app;
        return app.run();
    }

    std::string cmd = argv[1];

    if (cmd == "c") return cmdCompress(argc, argv);
    if (cmd == "x") return cmdExtract(argc, argv);
    if (cmd == "l") return cmdList(argc, argv);

    printUsage(argv[0]);
    return 1;
}
