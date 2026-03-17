#include "file_picker.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <sstream>

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
    if (std::filesystem::is_directory(dir)) {
        current_dir_ = std::filesystem::canonical(dir);
        focused_ = 0;
        refresh();
    }
}

void FilePicker::clearSelection() {
    selected_.clear();
}

void FilePicker::refresh() {
    entries_.clear();

    if (current_dir_.has_parent_path() && current_dir_ != current_dir_.root_path()) {
        FileEntry parent;
        parent.display_name = "..";
        parent.full_path = current_dir_.parent_path();
        parent.is_directory = true;
        entries_.push_back(std::move(parent));
    }

    std::vector<FileEntry> dirs;
    std::vector<FileEntry> files;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(current_dir_, ec)) {
        if (!config_.show_hidden) {
            auto name = entry.path().filename().string();
            if (!name.empty() && name[0] == '.') continue;
        }

        FileEntry fe;
        fe.full_path = entry.path();
        fe.display_name = entry.path().filename().string();
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
    auto filename = p.filename().string();
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
            self->refresh();
        }

        Elements rows;

        for (int i = 0; i < static_cast<int>(self->entries_.size()); ++i) {
            const auto& entry = self->entries_[i];
            bool is_selected = self->selected_.count(entry.full_path) > 0;
            bool is_focused_row = (i == self->focused_);

            std::string prefix;
            if (self->config_.multi_select && entry.display_name != "..") {
                prefix = is_selected ? "[x] " : "[ ] ";
            } else if (entry.is_directory) {
                prefix = "[D] ";
            } else {
                prefix = "    ";
            }

            std::string display_name = entry.display_name;
            if (entry.is_directory && entry.display_name != "..") {
                display_name += "/";
            }

            std::string size_str;
            if (!entry.is_directory && entry.display_name != "..") {
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
                row = row | bold | select;
            }

            rows.push_back(row);
        }

        if (rows.empty()) {
            rows.push_back(text("  (empty directory)") | dim);
        }

        auto file_list = vbox(std::move(rows)) | vscroll_indicator | yframe |
                         yflex;

        Elements content;
        content.push_back(hbox({
            text(" "),
            text(self->current_dir_.string()) | bold | color(Color::Yellow),
        }));
        content.push_back(separator());
        content.push_back(file_list | flex);
        content.push_back(separator());

        std::string hint;
        if (self->config_.multi_select) {
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

        if (event == Event::Character(' ') && self->config_.multi_select) {
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
                self->setDirectory(entry.full_path);
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
            if (self->current_dir_.has_parent_path() &&
                self->current_dir_ != self->current_dir_.root_path()) {
                self->setDirectory(self->current_dir_.parent_path());
            }
            return true;
        }

        return false;
    });
}

}  // namespace szip::tui
