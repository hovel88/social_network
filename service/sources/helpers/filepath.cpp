#include <format>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <system_error>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <unistd.h>
#include <sys/stat.h>
#include "helpers/filepath.h"

namespace SocialNetwork {

namespace FsHelpers {

FilePath::FilePath(const char* path)
{
    set_path(std::string(path));
}

FilePath::FilePath(const std::string& path)
{
    set_path(path);
}

FilePath& FilePath::operator=(const char* path)
{
    set_path(std::string(path));
    return *this;
}

FilePath& FilePath::operator=(const std::string& path)
{
    set_path(path);
    return *this;
}

void FilePath::swap(FilePath& file) noexcept
{
    std::swap(path_, file.path_);
}

std::string FilePath::self()
{
    std::string path;
    char buf[PATH_MAX + 1] {0};

    const std::size_t size = sizeof(buf);
    auto n = readlink("/proc/self/exe", buf, size);
    if (n > 0 && n < PATH_MAX) {
        path = buf;
    } else {
        path = "/";
    }
    return path;
}

std::string FilePath::current_dir()
{
    return std::filesystem::current_path().string();
}

std::string FilePath::temp_dir()
{
    return std::filesystem::temp_directory_path().string();
}

std::string FilePath::null_dev()
{
    return "/dev/null";
}

const std::string& FilePath::path() const
{
    return path_;
}

std::optional<std::string> FilePath::absolute_path() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        auto path = std::filesystem::absolute(fs_path, ec);
        if (!ec) return path.string();
    }
    return std::nullopt;
}

std::optional<std::string> FilePath::canonical_path() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        auto path = std::filesystem::weakly_canonical(fs_path, ec);
        if (!ec) return path.string();
    }
    return std::nullopt;
}

std::string FilePath::parent_path() const
{
    if (!path_.empty()) {
        const std::filesystem::path fs_path(path_);
        return fs_path.parent_path().string();
    }
    return {};
}

std::string FilePath::filename() const
{
    if (!path_.empty()) {
        const std::filesystem::path fs_path(path_);
        return fs_path.filename().string();
    }
    return {};
}

std::string FilePath::basename() const
{
    if (!path_.empty()) {
        const std::filesystem::path fs_path(path_);
        return fs_path.stem().string();
    }
    return {};
}

std::string FilePath::extension() const
{
    if (!path_.empty()) {
        const std::filesystem::path fs_path(path_);
        return fs_path.extension().string();
    }
    return {};
}

bool FilePath::can_read() const
{
    if (!path_.empty()) {
        struct stat st;
        if (stat(path_.c_str(), &st) == 0) {
            uid_t uid = geteuid();
            gid_t gid = getegid();
            if (st.st_uid == uid) {
                return (st.st_mode & S_IRUSR) != 0;
            } else
            if (st.st_gid == gid) {
                return (st.st_mode & S_IRGRP) != 0;
            } else {
                return (st.st_mode & S_IROTH) != 0 || uid == 0;
            }
        }
    }
    return false;
}

bool FilePath::can_write() const
{
    if (!path_.empty()) {
        struct stat st;
        if (stat(path_.c_str(), &st) == 0) {
            uid_t uid = geteuid();
            gid_t gid = getegid();
            if (st.st_uid == uid) {
                return (st.st_mode & S_IWUSR) != 0;
            } else
            if (st.st_gid == gid) {
                return (st.st_mode & S_IWGRP) != 0;
            } else {
                return (st.st_mode & S_IWOTH) != 0 || uid == 0;
            }
        }
    }
    return false;
}

bool FilePath::can_execute() const
{
    if (!path_.empty()) {
        const auto abs_path = absolute_path();
        if (abs_path.has_value()) {
            const auto path = abs_path.value();
            if (FilePath(path).exists()) {
                struct stat st;
                if (stat(path.c_str(), &st) == 0) {
                    uid_t uid = geteuid();
                    gid_t gid = getegid();
                    if (st.st_uid == uid || uid == 0) {
                        return (st.st_mode & S_IXUSR) != 0;
                    } else
                    if (st.st_gid == gid) {
                        return (st.st_mode & S_IXGRP) != 0;
                    } else {
                        return (st.st_mode & S_IXOTH) != 0;
                    }
                }
            }
        }
    }
    return false;
}

FilePath& FilePath::set_writeable(bool flag)
{
    if (path_.empty()) throw std::runtime_error(std::format("set_writeable(): path is empty"));

    std::error_code ec;
    const std::filesystem::path fs_path(path_);
    const std::filesystem::perms cur_perm = std::filesystem::status(fs_path).permissions();
    std::filesystem::perms fs_perm;
    if (flag) {
        fs_perm = cur_perm | std::filesystem::perms::owner_write;
    } else {
        std::filesystem::perms mask = std::filesystem::perms::owner_write
                                    | std::filesystem::perms::group_write
                                    | std::filesystem::perms::others_write;
        fs_perm = cur_perm & ~mask;
    }
    std::filesystem::permissions(fs_path, fs_perm, ec);

    if (ec) throw std::runtime_error(std::format("set_writeable(): {}", ec.message()));
    return *this;
}

FilePath& FilePath::set_executable(bool flag)
{
    if (path_.empty()) throw std::runtime_error(std::format("set_executable(): path is empty"));

    std::error_code ec;
    const std::filesystem::path fs_path(path_);
    const std::filesystem::perms cur_perm = std::filesystem::status(fs_path).permissions();
    std::filesystem::perms fs_perm;
    if (flag) {
        fs_perm = cur_perm | std::filesystem::perms::owner_exec;
        if ((cur_perm & std::filesystem::perms::group_read) == std::filesystem::perms::group_read) {
            fs_perm |= std::filesystem::perms::group_exec;
        }
        if ((cur_perm & std::filesystem::perms::others_read) == std::filesystem::perms::others_read) {
            fs_perm |= std::filesystem::perms::others_exec;
        }
    } else {
        std::filesystem::perms mask = std::filesystem::perms::owner_exec
                                    | std::filesystem::perms::group_exec
                                    | std::filesystem::perms::others_exec;
        fs_perm = cur_perm & ~mask;
    }
    std::filesystem::permissions(fs_path, fs_perm, ec);

    if (ec) throw std::runtime_error(std::format("set_executable(): {}", ec.message()));
    return *this;
}

std::optional<uint32_t> FilePath::permissions() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        if (std::filesystem::exists(fs_path, ec)) {
            std::filesystem::perms cur_perm = std::filesystem::status(fs_path).permissions();
            if (!ec) return static_cast<uint32_t>(cur_perm);
        }
    }
    return std::nullopt;
}

FilePath& FilePath::set_file_permissions(uint32_t perms)
{
    if (path_.empty()) throw std::runtime_error(std::format("set_file_permissions(): path is empty"));

    std::filesystem::perms fs_perm = static_cast<std::filesystem::perms>(perms & static_cast<uint32_t>(std::filesystem::perms::mask));
    std::error_code ec;
    const std::filesystem::path fs_path(path_);
    if (std::filesystem::is_regular_file(fs_path, ec)
    ||  std::filesystem::is_symlink(fs_path, ec)) {
        std::filesystem::permissions(fs_path, fs_perm, ec);
    }

    if (ec) throw std::runtime_error(std::format("set_file_permissions(): {}", ec.message()));
    return *this;
}

FilePath& FilePath::set_directory_permissions(uint32_t perms, bool recursive)
{
    if (path_.empty()) throw std::runtime_error(std::format("set_directory_permissions(): path is empty"));

    std::filesystem::perms fs_perm = static_cast<std::filesystem::perms>(perms & static_cast<uint32_t>(std::filesystem::perms::mask));
    std::error_code ec;
    const std::filesystem::path fs_path(path_);
    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(fs_path)) {
            if (std::filesystem::is_directory(entry, ec)) {
                std::filesystem::permissions(entry.path(), fs_perm, ec);
            }
        }
    } else {
        if (std::filesystem::is_directory(fs_path, ec)) {
            std::filesystem::permissions(fs_path, fs_perm, ec);
        }
    }

    if (ec) throw std::runtime_error(std::format("set_directory_permissions(): {}", ec.message()));
    return *this;
}

bool FilePath::hidden() const
{
    if (!path_.empty()) {
        const std::filesystem::path fs_path(path_);
        std::string filename = fs_path.filename().string();
        return filename.size() > 0 && filename[0] == '.';
    }
    return false;
}

bool FilePath::exists() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        auto rv = std::filesystem::exists(fs_path, ec);

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::exists_and_empty() const
{
    if (!path_.empty()) {
        bool rv = false;
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        if (std::filesystem::exists(fs_path, ec)) {
            rv = std::filesystem::is_empty(fs_path, ec);
        }

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::is_file() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        auto rv = std::filesystem::is_regular_file(fs_path, ec);

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::is_link() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        auto rv = std::filesystem::is_symlink(fs_path, ec);

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::is_directory() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        auto rv = std::filesystem::is_directory(fs_path, ec);

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::is_other() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        auto rv = std::filesystem::is_other(fs_path, ec);

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::file_exists() const
{
    return exists() && is_file();
}

bool FilePath::link_exists() const
{
    return exists() && is_link();
}

bool FilePath::directory_exists() const
{
    return exists() && is_directory();
}

bool FilePath::link(const std::string& path_to, bool hardlink, bool recursive)
{
    if (!path_.empty() && !path_to.empty()) {
        bool rv = false;
        std::error_code ec;
        const std::filesystem::path fs_target_path(path_);
        const std::filesystem::path fs_link_path(path_to);

        if (hardlink) {
            // for hardlink the pathname TARGET_PATH must exist
            if (std::filesystem::exists(fs_target_path, ec)) {
                if (std::filesystem::is_other(fs_target_path, ec)) {
                    // not a regular file, a directory, or a symlink
                    rv = false;
                } else
                if (std::filesystem::is_directory(fs_target_path, ec)) {
                    if (recursive) {
                        for (const auto& entry : std::filesystem::recursive_directory_iterator(fs_target_path)) {
                            const auto& entry_path = entry.path();
                            if (std::filesystem::is_regular_file(entry_path, ec)
                            ||  std::filesystem::is_symlink(entry_path, ec)) {
                                // создаем путь (структуру каталогов)
                                const auto src_relative_path = std::filesystem::relative(entry_path, fs_target_path);
                                const auto dst_parent_path   = fs_link_path / src_relative_path.parent_path();
                                const auto dst_file_path     = fs_link_path / src_relative_path;
                                std::filesystem::create_directories(dst_parent_path, ec);
                                // в которую помещаем ссылки
                                std::filesystem::create_hard_link(entry_path, dst_file_path, ec);
                            }
                        }
                        rv = true;
                    } else {
                        rv = false;
                    }
                } else {
                    std::filesystem::create_hard_link(fs_target_path, fs_link_path, ec);
                    rv = true;
                }
            }
        } else {
            // for symlink the pathname TARGET_PATH may be invalid or non-existing
            // Note: TARGET_PATH must be an absolute path unless LINK_PATH is in the current directory
            std::filesystem::create_symlink(fs_target_path, fs_link_path, ec);
            rv = true;
        }

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::copy(const std::string& path_to, bool recursive)
{
    if (!path_.empty() && !path_to.empty()) {
        bool rv = false;
        std::error_code ec;
        const std::filesystem::path fs_from_path(path_);
        const std::filesystem::path fs_to_path(path_to);

        // copy_options:
        // - when the file already exists
        //   * none                 Report an error (default behavior for copy_file())
        //   * skip_existing        Keep the existing file, without reporting an error
        //   * overwrite_existing   Replace the existing file
        //   * update_existing      Replace the existing file only if it is older than
        //                          the file being copied
        // - on subdirectories
        //   * none                 Skip subdirectories (default behavior for copy())
        //   * recursive            Recursively copy subdirectories and their content
        // - on symbolic links
        //   * none                 Follow symlinks (default behavior for copy())
        //   * copy_symlinks        Copy symlinks as symlinks, not as files they point to
        //   * skip_symlinks        Ignore symlinks
        // - the kind of copying
        //   * none                 Copy file content (default behavior for copy())
        //   * directories_only     Copy the directory structure, but do not copy any
        //                          non-directory files
        //   * create_symlinks      Instead of creating copies of files, create symlinks
        //                          pointing to the originals.
        //                          NOTE: the source path must be an absolute path unless
        //                          the destination path is in the current directory
        //   * create_hard_links    Instead of creating copies of files, create hardlinks
        //                          that resolve to the same files as the originals
        if (std::filesystem::exists(fs_from_path, ec)) {
            if (std::filesystem::is_other(fs_from_path, ec)) {
                // not a regular file, a directory, or a symlink
                rv = false;
            } else
            if (std::filesystem::is_symlink(fs_from_path, ec)) {
                std::filesystem::copy_symlink(fs_from_path, fs_to_path, ec);
                rv = true;
            } else
            if (std::filesystem::is_regular_file(fs_from_path, ec)) {
                std::filesystem::copy_options opt = std::filesystem::copy_options::overwrite_existing;
                if (std::filesystem::exists(fs_to_path, ec)
                &&  std::filesystem::is_directory(fs_to_path, ec)) {
                    // Behavior:
                    // - if FROM_PATH is a regular file and if TO_PATH is a directory,
                    //   then behaves as if copy_file(from, to/from.filename(), options)
                    //   (creates a copy of from as a file in the directory to)
                    std::filesystem::copy(fs_from_path, fs_to_path, opt, ec);
                    rv = true;
                } else {
                    // The behavior is undefined if there is more than one option in any of the copy_options.
                    // Behavior:
                    // - if the destination does not exist,
                    //   copies the contents and the attributes of the FROM_PATH to the TO_PATH
                    //   (symlinks are followed).
                    // - if the destination already exists,
                    //   * error = TO_PATH and FROM_PATH are the same
                    //   * error = TO_PATH is not a regular file
                    //   * error = if copy_options::none is set
                    //   * nothing = if copy_options::skip_existing is set
                    //   * copy the file = if copy_options::overwrite_existing is set
                    //   * copy the file = if copy_options::update_existing is set and FROM_PATH is newer
                    //     than TO_PATH
                    if (std::filesystem::copy_file(fs_from_path, fs_to_path, opt, ec)) {
                        rv = true;
                    }
                }
            } else
            if (std::filesystem::is_directory(fs_from_path, ec)) {
                // The behavior is undefined if there is more than one option in any of the copy_options.
                // Behavior:
                // - if the destination does not exist,
                //   creates the new directory with a copy of the old directory's attributes
                //   if either copy_options::recursive or is copy_options::none is set.
                // - if the destination already exists,
                //   * error = TO_PATH and FROM_PATH are the same
                //   * error = TO_PATH is a regular file
                //   * error = if copy_options::create_symlinks is set
                //   * iterates over the files contained in FROM_PATH and for each directory entry,
                //     recursively calls copy(x.path(), to/x.path().filename())
                std::filesystem::copy_options opt = std::filesystem::copy_options::overwrite_existing;
                if (recursive) opt |= std::filesystem::copy_options::recursive;
                std::filesystem::copy(fs_from_path, fs_to_path, opt, ec);
                rv = true;
            }

            if (ec) rv = false;
            return rv;
        }
    }
    return false;
}

bool FilePath::move(const std::string& path_to)
{
    if (!path_.empty() && !path_to.empty()) {
        bool rv = false;
        std::error_code ec;
        const std::filesystem::path fs_from_path(path_);
        const std::filesystem::path fs_to_path(path_to);

        if (std::filesystem::exists(fs_from_path, ec)) {
            if (std::filesystem::is_other(fs_from_path, ec)) {
                // not a regular file, a directory, or a symlink
                rv = false;
            } else {
                // Symlinks are not followed:
                // - if FROM_PATH is a symlink, it is itself renamed, not its target
                // - if TO_PATH is an existing symlink, it is itself erased, not its target
                // Behavior:
                // - if FROM_PATH is a non-directory,
                //   * nothing = if TO_PATH the same file as FROM_PATH or a hardlink to it
                //   * error = if TO_PATH ends with 'dot' or with 'dot-dot'
                //   * TO_PATH is existing non-directory file:
                //     -- TO_PATH file is first deleted,
                //     -- the pathname TO_PATH is linked to the file,
                //     -- FROM_PATH is unlinked from the file
                //   * TO_PATH non-existing file in an existing directory:
                //     -- the pathname TO_PATH is linked to the file,
                //     -- FROM_PATH is unlinked from the file
                // - if FROM_PATH is a directory,
                //   * nothing = if TO_PATH the same directory as FROM_PATH or a hardlink to it
                //   * error = if TO_PATH ends with 'dot' or with 'dot-dot'
                //   * error = if TO_PATH names a non-existing directory ending with a directory separator
                //   * error = if FROM_PATH is a directory which is an ancestor of TO_PATH
                //   * TO_PATH is existing directory:
                //     -- TO_PATH is first deleted if empty (error if not empty),
                //     -- the pathname TO_PATH is linked to the directory,
                //     -- FROM_PATH is unlinked from the directory
                //   * TO_PATH non-existing directory and whose parent directory exists:
                //     -- the pathname TO_PATH is linked to the directory,
                //     -- FROM_PATH is unlinked from the directory
                std::filesystem::rename(fs_from_path, fs_to_path, ec);
                rv = true;
            }
        }

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::remove(bool recursive)
{
    if (!path_.empty()) {
        bool rv = false;
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        if (std::filesystem::is_other(fs_path, ec)) {
            // not a regular file, a directory, or a symlink
            rv = false;
        } else
        if (std::filesystem::is_symlink(fs_path, ec)
        ||  std::filesystem::is_regular_file(fs_path, ec)) {
            // On POSIX systems, this function typically calls unlink and rmdir as needed
            rv = std::filesystem::remove(fs_path, ec);
        } else
        if (std::filesystem::is_directory(fs_path, ec)) {
            if (recursive) {
                rv = (std::filesystem::remove_all(fs_path, ec) > 0);
            } else {
                rv = std::filesystem::remove(fs_path, ec);
            }
        }

        if (ec) rv = false;
        return rv;
    }
    return false;
}

bool FilePath::create_file()
{
    static const std::filesystem::perms permissions = std::filesystem::perms::owner_read  | std::filesystem::perms::owner_write
                                                    | std::filesystem::perms::group_read  | std::filesystem::perms::group_write
                                                    | std::filesystem::perms::others_read; // 0664
    const uint32_t fs_perm = static_cast<uint32_t>(permissions);
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        if (std::filesystem::exists(fs_path, ec)) {
            return false;
        } else {
            std::fstream fs;
            fs.open(path_, std::ios::out | std::ios::trunc);
            fs.close();
            if (std::filesystem::is_regular_file(fs_path, ec)) {
                set_file_permissions(fs_perm);
            }
        }
        if (!ec) return true;
    }
    return false;
}

bool FilePath::create_directory(bool full_path)
{
    static const std::filesystem::perms permissions = std::filesystem::perms::owner_all
                                                    | std::filesystem::perms::group_all
                                                    | std::filesystem::perms::others_all; // 0777
    const uint32_t fs_perm = static_cast<uint32_t>(permissions);
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        if (std::filesystem::exists(fs_path, ec)) {
            return false;
        }
        if (full_path) {
            if (std::filesystem::create_directories(fs_path, ec)) {
                set_directory_permissions(fs_perm, /*recursive=*/false);
            }
        } else {
            if (std::filesystem::create_directory(fs_path, ec)) {
                set_directory_permissions(fs_perm, /*recursive=*/false);
            }
        }
        if (!ec) return true;
    }
    return false;
}

int64_t FilePath::get_created_ts() const
{
    if (!path_.empty()) {
        struct stat st;
        if (stat(path_.c_str(), &st) == 0) {
            // в секундах (UTC time value)
            return static_cast<int64_t>(st.st_ctim.tv_sec);
        }
    }
    return -1;
}

int64_t FilePath::get_last_modified_ts() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        const auto tp = std::filesystem::last_write_time(fs_path, ec);
        // в секундах (UTC time value)
        return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    }
    return -1;
}

FilePath& FilePath::set_last_modified_ts(const int64_t& ts)
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        const std::filesystem::file_time_type tp{std::chrono::seconds(ts)};
        // в секундах (UTC time value)
        std::filesystem::last_write_time(fs_path, tp, ec);
    }
    return *this;
}

size_t FilePath::get_size() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        return std::filesystem::file_size(fs_path, ec);
    }
    return 0;
}

FilePath& FilePath::set_size(size_t size)
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        std::filesystem::resize_file(fs_path, size);
    }
    return *this;
}

uint64_t FilePath::total_space() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path = path_.c_str();
        const std::filesystem::space_info si = std::filesystem::space(fs_path, ec);
        return si.capacity;
    }
    return 0;
}

uint64_t FilePath::available_space() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path = path_.c_str();
        const std::filesystem::space_info si = std::filesystem::space(fs_path, ec);
        return si.available;
    }
    return 0;
}

uint64_t FilePath::free_space() const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path = path_.c_str();
        const std::filesystem::space_info si = std::filesystem::space(fs_path, ec);
        return si.free;
    }
    return 0;
}

void FilePath::list_files(std::vector<std::string>& items, const std::string& filter) const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        if (std::filesystem::exists(fs_path, ec)
        &&  std::filesystem::is_directory(fs_path, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(fs_path)) {
                const auto& entry_path = entry.path();
                if (!std::filesystem::is_regular_file(entry_path, ec)
                &&  !std::filesystem::is_symlink(entry_path, ec)) {
                    continue;
                }

                // пробегаемся по всем регулярным файлам директории
                // ищем файлы, соответствующие фильтру,
                // либо все файлы, если фильтр не задан
                std::string filename = entry_path.filename().string();
                if (!filter.empty() && filename.find(filter) == std::string::npos) {
                    continue;
                }
                items.push_back(entry_path.string());
            }
        }
    }
}

void FilePath::list_files(std::vector<FilePath>& items, const std::string& filter) const
{
    std::vector<std::string> files;
    list_files(files, filter);
    for (const auto& one : files) {
        items.push_back(FilePath(one));
    }
}

void FilePath::list_subdirs(std::vector<std::string>& items, const std::string& filter) const
{
    if (!path_.empty()) {
        std::error_code ec;
        const std::filesystem::path fs_path(path_);
        if (std::filesystem::exists(fs_path, ec)
        &&  std::filesystem::is_directory(fs_path, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(fs_path)) {
                const auto& entry_path = entry.path();
                if (!std::filesystem::is_directory(entry_path, ec)) {
                    continue;
                }

                // пробегаемся по всем регулярным файлам директории
                // ищем файлы, соответствующие фильтру,
                // либо все файлы, если фильтр не задан
                std::string filename = entry_path.filename().string();
                if (!filter.empty() && filename.find(filter) == std::string::npos) {
                    continue;
                }
                items.push_back(entry_path.string());
            }
        }
    }
}

void FilePath::list_subdirs(std::vector<FilePath>& items, const std::string& filter) const
{
    std::vector<std::string> subdirs;
    list_subdirs(subdirs, filter);
    for (const auto& one : subdirs) {
        items.push_back(FilePath(one));
    }
}

void FilePath::set_path(const std::string& path)
{
    path_ = path;
    std::string::size_type n = path_.size();
    if (n > 1 && path_[n - 1] == '/') {
        path_.resize(n - 1);
    }
}

} // namespace FsHelpers

} // namespace SocialNetwork
