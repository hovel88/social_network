#pragma once

#include <string>
#include <optional>
#include <vector>

namespace SocialNetwork {

namespace FsHelpers {

class FilePath
{
public:
    ~FilePath() = default;
    FilePath() = default;
    FilePath(const FilePath&) = default;
    FilePath(FilePath&&) noexcept = default;
    FilePath& operator=(const FilePath&) = default;
    FilePath& operator=(FilePath&&) noexcept = default;

    explicit FilePath(const char* path);
    explicit FilePath(const std::string& path);
    FilePath& operator=(const char* path);
    FilePath& operator=(const std::string& path);

    void swap(FilePath& file) noexcept;

    static std::string self();        // path to the executable file
    static std::string current_dir(); // path to the current working directory
    static std::string temp_dir();    // path to the temporary directory
    static std::string null_dev();    // path to the null device

    const std::string& path() const;
    std::optional<std::string> absolute_path() const;
    std::optional<std::string> canonical_path() const;
    std::string parent_path() const;
    std::string filename() const;
    std::string basename() const;
    std::string extension() const;

    bool can_read() const;
    bool can_write() const;
    bool can_execute() const;
    FilePath& set_writeable(bool flag = true);
    FilePath& set_executable(bool flag = true);

    std::optional<uint32_t> permissions() const;
    FilePath& set_file_permissions(uint32_t perms);
    FilePath& set_directory_permissions(uint32_t perms, bool recursive = false);

    bool hidden() const;
    bool exists() const;
    bool exists_and_empty() const;

    bool is_file() const;
    bool is_link() const;
    bool is_directory() const;
    bool is_other() const; // file that is not a regular file, a directory, or a symlink

    bool file_exists() const;
    bool link_exists() const;
    bool directory_exists() const;

    bool link(const std::string& path_to, bool hardlink = false, bool recursive = false);
    bool copy(const std::string& path_to, bool recursive = false);
    bool move(const std::string& path_to); // equivalent to rename
    bool remove(bool recursive = false);

    bool create_file();
    bool create_directory(bool full_path = false);

    int64_t get_created_ts() const;         // returns UNIX-time of this entry creation time, or -1
    int64_t get_last_modified_ts() const;   // returns UNIX-time of this entry last modified time, or -1
    FilePath& set_last_modified_ts(const int64_t& ts);

    size_t get_size() const;
    FilePath& set_size(size_t size);

    uint64_t total_space() const;     // total size in bytes of the partition containing this path
    uint64_t available_space() const; // number of available bytes on the partition containing this path
    uint64_t free_space() const;      // number of free bytes on the partition containing this path

    // TODO: вместо тривиального строкового фильтра лучше передать функцию-предикат, которую
    // в вызывающем коде можно оформить в виде замыкания и реализовать любую сложную логику
    void list_files(std::vector<std::string>& items, const std::string& filter) const;
    void list_files(std::vector<FilePath>& items, const std::string& filter) const;
    void list_subdirs(std::vector<std::string>& items, const std::string& filter) const;
    void list_subdirs(std::vector<FilePath>& items, const std::string& filter) const;

private:
    std::string path_{};

    void set_path(const std::string& path);
};

inline void swap(FilePath& f1, FilePath& f2) noexcept
{
    f1.swap(f2);
}

} // namespace FsHelpers

} // namespace SocialNetwork
