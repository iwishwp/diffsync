#include "diffsync/types.hpp"
#include <sstream>
#include <stdexcept>

namespace diffsync {

std::string toString(FileStatus status) {
    switch (status) {
        case FileStatus::Modified:   return "M";
        case FileStatus::Staged:     return "M ";  // Two chars to distinguish
        case FileStatus::Added:      return "A";
        case FileStatus::Untracked:  return "??";
        case FileStatus::Deleted:    return "D";
        default:                     return "?";
    }
}

FileStatus parseFileStatus(const std::string& statusStr) {
    if (statusStr == "M")  return FileStatus::Modified;
    if (statusStr == "A")  return FileStatus::Added;
    if (statusStr == "??") return FileStatus::Untracked;
    if (statusStr == "D")  return FileStatus::Deleted;
    if (statusStr == "M ") return FileStatus::Staged;
    throw std::invalid_argument("Unknown file status: " + statusStr);
}

std::string ChangeFile::statusString() const {
    return toString(status);
}

bool RemoteConfig::isValid() const {
    if (name.empty()) return false;
    if (host.empty()) return false;
    if (user.empty()) return false;
    if (remotePath.empty()) return false;
    if (port == 0 || port > 65535) return false;
    return true;
}

std::string RemoteConfig::toString() const {
    std::ostringstream oss;
    oss << user << "@" << host << ":" << port << remotePath;
    return oss.str();
}

} // namespace diffsync
