#include "file_picker.h"
#include "utf8_utils.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace szip::tui {

using namespace ftxui;

FilePicker::FilePicker(FilePickerConfig config)
    : config_(std::move(config)) {
    current_dir_ = std::filesystem::current_path();
    refresh();
}

void FilePicker::onSubmit(OnSubmit cb) {
    on_submit_ = std::move(cb);
}

std::filesystem::path FilePicker::currentDirectory() const {
    return current_dir_;
}

std::vector<std::filesystem::path> FilePicker::selectedPaths() const {
    return {selected_.begin(), selected_.end()};
}

std::filesystem::path FilePicker::focusedPath() const {
    if (focused_ >= 0 && focused_ < static_cast<int>(entries_.size())) {
        return entries_[focused_].full_path;
    }
    return {};
}

void FilePicker::setDirectory(const std::filesystem::path& dir) {
    std::error_code ec;
    if (std::filesystem::is_directory(dir, ec)) {
        auto canon = std::filesystem::canonical(dir, ec);
        current_dir_ = ec ? dir : canon;
        at_drive_list_ = false;
        focused_ = 0;
        refresh();
    }
}

void FilePicker::clearSelection() {
    selected_.clear();
}

void FilePicker::refreshDriveList() {
#ifdef _WIN32
    entries_.clear();
    at_drive_list_ = true;

    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1u << i)) {
            FileEntry fe;
            std::string root = std::string(1, static_cast<char>('A' + i)) + ":\\";
            fe.display_name = root;
            fe.full_path = std::filesystem::path(root);
            fe.is_directory = true;
            entries_.push_back(std::move(fe));
        }
    }
    focused_ = 0;
    last_refresh_ = std::chrono::steady_clock::now();
#endif
}

void FilePicker::refresh() {
    entries_.clear();
    at_drive_list_ = false;

    bool at_root = (current_dir_ == current_dir_.root_path());

    if (current_dir_.has_parent_path() && !at_root) {
        FileEntry parent;
        parent.display_name = "..";
        parent.full_path = current_dir_.parent_path();
        parent.is_directory = true;
        entries_.push_back(std::move(parent));
    }
#ifdef _WIN32
    else if (at_root) {
        FileEntry parent;
        parent.display_name = "..";
        parent.is_directory = true;
        entries_.push_back(std::move(parent));
    }
#endif

    std::vector<FileEntry> dirs;
    std::vector<FileEntry> files;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(current_dir_, ec)) {
        if (!config_.show_hidden) {
            auto name = pathToUtf8(entry.path().filename());
            if (!name.empty() && name[0] == '.') continue;
        }

        FileEntry fe;
        fe.full_path = entry.path();
        fe.display_name = pathToUtf8(entry.path().filename());
        fe.is_directory = entry.is_directory(ec);

        if (fe.is_directory) {
            dirs.push_back(std::move(fe));
        } else {
            if (!matchesFilter(entry.path())) continue;
            fe.size = entry.file_size(ec);
            files.push_back(std::move(fe));
        }
    }

    std::sort(dirs.begin(), dirs.end(),
              [](const auto& a, const auto& b) { return a.display_name < b.display_name; });
    std::sort(files.begin(), files.end(),
              [](const auto& a, const auto& b) { return a.display_name < b.display_name; });

    for (auto& d : dirs) entries_.push_back(std::move(d));
    for (auto& f : files) entries_.push_back(std::move(f));

    if (focused_ >= static_cast<int>(entries_.size())) {
        focused_ = entries_.empty() ? 0 : static_cast<int>(entries_.size()) - 1;
    }
    last_refresh_ = std::chrono::steady_clock::now();
}

bool FilePicker::matchesFilter(const std::filesystem::path& p) const {
    if (config_.extension_filter.empty()) return true;
    auto filename = pathToUtf8(p.filename());
    for (const auto& ext : config_.extension_filter) {
        if (filename.size() >= ext.size() &&
            filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

std::string FilePicker::formatSize(std::uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        std::ostringstream oss;
        oss.precision(1);
        oss << std::fixed << static_cast<double>(bytes) / 1024.0 << " KB";
        return oss.str();
    }
    if (bytes < 1024ULL * 1024 * 1024) {
        std::ostringstream oss;
        oss.precision(1);
        oss << std::fixed << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB";
        return oss.str();
    }
    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed << static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0) << " GB";
    return oss.str();
}

Component FilePicker::component() {
    auto self = this;

    auto renderer = Renderer([self](bool focused) {
        auto now = std::chrono::steady_clock::now();
        if (now - self->last_refresh_ > std::chrono::seconds(2)) {
            if (self->at_drive_list_)
                self->refreshDriveList();
            else
                self->refresh();
        }

        auto term_size = Terminal::Size();
        int max_name_width = std::max(20, term_size.dimx / 2);

        Elements rows;

        for (int i = 0; i < static_cast<int>(self->entries_.size()); ++i) {
            const auto& entry = self->entries_[i];
            bool is_selected = self->selected_.count(entry.full_path) > 0;
            bool is_focused_row = (i == self->focused_);

            std::string prefix;
            if (self->config_.multi_select && entry.display_name != ".." &&
                !self->at_drive_list_) {
                prefix = is_selected ? "[x] " : "[ ] ";
            } else if (entry.is_directory) {
                prefix = "[D] ";
            } else {
                prefix = "    ";
            }

            std::string display_name = entry.display_name;
            if (entry.is_directory && entry.display_name != ".." &&
                !self->at_drive_list_) {
                display_name += "/";
            }
            display_name = truncateToWidth(display_name, max_name_width);

            std::string size_str;
            if (self->at_drive_list_) {
                // drives show no size
            } else if (!entry.is_directory && entry.display_name != "..") {
                size_str = formatSize(entry.size);
            } else if (entry.is_directory && entry.display_name != "..") {
                size_str = "<DIR>";
            }

            auto row = hbox({
                text(prefix),
                text(display_name) | flex,
                text(size_str) | dim | align_right,
            });

            if (is_selected && !is_focused_row) {
                row = row | color(Color::Cyan);
            }

            if (is_focused_row && focused) {
                row = row | inverted | focus;
            } else if (is_focused_row) {
                row = row | bold | ftxui::select;
            }

            rows.push_back(row);
        }

        if (rows.empty()) {
            rows.push_back(text("  (empty directory)") | dim);
        }

        auto file_list = vbox(std::move(rows)) | vscroll_indicator | yframe |
                         yflex;

        Elements content;

        if (self->at_drive_list_) {
            content.push_back(hbox({
                text(" "),
                text("Select Drive") | bold | color(Color::Yellow),
            }));
        } else {
            auto path_str = pathToUtf8(self->current_dir_);
            path_str = truncateToWidth(path_str, std::max(10, term_size.dimx - 4));
            content.push_back(hbox({
                text(" "),
                text(path_str) | bold | color(Color::Yellow),
            }));
        }

        content.push_back(separator());
        content.push_back(file_list | flex);
        content.push_back(separator());

        std::string hint;
        if (self->at_drive_list_) {
            hint = " Enter:open drive  Backspace:return";
        } else if (self->config_.multi_select) {
            hint = " Space:select  Enter:open dir  Backspace:up  ";
            auto sel_count = self->selected_.size();
            if (sel_count > 0) {
                std::uintmax_t total = 0;
                for (const auto& p : self->selected_) {
                    std::error_code ec;
                    if (std::filesystem::is_regular_file(p, ec))
                        total += std::filesystem::file_size(p, ec);
                }
                hint += "| " + std::to_string(sel_count) + " selected (" +
                        formatSize(total) + ")";
            }
        } else {
            hint = " Enter:select/open  Backspace:up";
        }
        content.push_back(text(hint) | dim);

        return vbox(std::move(content)) | border | flex;
    });

    return CatchEvent(renderer, [self](Event event) {
        if (self->entries_.empty()) return false;

        if (event == Event::ArrowUp || event == Event::Character('k')) {
            if (self->focused_ > 0) --self->focused_;
            return true;
        }
        if (event == Event::ArrowDown || event == Event::Character('j')) {
            if (self->focused_ < static_cast<int>(self->entries_.size()) - 1)
                ++self->focused_;
            return true;
        }

        if (event == Event::PageUp) {
            self->focused_ = std::max(0, self->focused_ - 10);
            return true;
        }
        if (event == Event::PageDown) {
            self->focused_ = std::min(
                static_cast<int>(self->entries_.size()) - 1, self->focused_ + 10);
            return true;
        }

        if (event == Event::Home) {
            self->focused_ = 0;
            return true;
        }
        if (event == Event::End) {
            self->focused_ = static_cast<int>(self->entries_.size()) - 1;
            return true;
        }

        if (event == Event::Character(' ') && self->config_.multi_select &&
            !self->at_drive_list_) {
            const auto& entry = self->entries_[self->focused_];
            if (entry.display_name != "..") {
                if (self->selected_.count(entry.full_path)) {
                    self->selected_.erase(entry.full_path);
                } else {
                    self->selected_.insert(entry.full_path);
                }
                if (self->focused_ < static_cast<int>(self->entries_.size()) - 1)
                    ++self->focused_;
            }
            return true;
        }

        if (event == Event::Return) {
            const auto& entry = self->entries_[self->focused_];
            if (entry.is_directory) {
                if (entry.full_path.empty()) {
                    self->refreshDriveList();
                } else {
                    self->setDirectory(entry.full_path);
                }
            } else if (!self->config_.multi_select) {
                self->selected_.clear();
                self->selected_.insert(entry.full_path);
                if (self->on_submit_) {
                    self->on_submit_(entry.full_path);
                }
            }
            return true;
        }

        if (event == Event::Backspace) {
            if (self->at_drive_list_) return true;
            if (self->current_dir_ == self->current_dir_.root_path()) {
                self->refreshDriveList();
                return true;
            }
            if (self->current_dir_.has_parent_path()) {
                self->setDirectory(self->current_dir_.parent_path());
            }
            return true;
        }

        return false;
    });
}

}  // namespace szip::tui
