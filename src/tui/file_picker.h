#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace szip::tui {

struct FilePickerConfig {
    bool multi_select = false;
    std::vector<std::string> extension_filter;
    bool show_hidden = false;
};

struct FileEntry {
    std::string display_name;
    std::filesystem::path full_path;
    bool is_directory = false;
    std::uintmax_t size = 0;
};

class FilePicker {
public:
    explicit FilePicker(FilePickerConfig config = {});

    ftxui::Component component();

    [[nodiscard]] std::filesystem::path currentDirectory() const;
    [[nodiscard]] std::vector<std::filesystem::path> selectedPaths() const;
    [[nodiscard]] std::filesystem::path focusedPath() const;

    void setDirectory(const std::filesystem::path& dir);
    void clearSelection();

    using OnSubmit = std::function<void(const std::filesystem::path&)>;
    void onSubmit(OnSubmit cb);

    static std::string formatSize(std::uintmax_t bytes);

private:
    FilePickerConfig config_;
    std::filesystem::path current_dir_;
    std::vector<FileEntry> entries_;
    int focused_ = 0;
    std::set<std::filesystem::path> selected_;
    OnSubmit on_submit_;

    void refresh();
    void refreshDriveList();
    bool matchesFilter(const std::filesystem::path& p) const;
    bool at_drive_list_ = false;
    std::chrono::steady_clock::time_point last_refresh_{};
};

}  // namespace szip::tui
