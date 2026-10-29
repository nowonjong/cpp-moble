#include "ProfileManager.h"

#include "RecordManager.h"
#include "Utf8Util.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {
constexpr const char* kHeader = "WORDCHAIN_PROFILES_V1";

std::filesystem::path unusedSibling(const std::filesystem::path& path) {
    for (int index = 0; index < 10000; ++index) {
        auto candidate = path;
        candidate += ".tmp." + std::to_string(index);
        std::error_code error;
        const bool exists = std::filesystem::exists(candidate, error);
        if (error) return {};
        if (!exists) return candidate;
    }
    return {};
}

bool parseNamedLine(const std::string& line, const char* expectedToken,
                    std::string& name) {
    std::istringstream input(line);
    std::string token;
    if (!(input >> token) || token != expectedToken) return false;
    input >> std::ws;
    if (input.peek() != '"' || !(input >> std::quoted(name))) return false;
    input >> std::ws;
    return input.eof();
}

bool replaceFile(const std::filesystem::path& source,
                 const std::filesystem::path& destination) {
#ifdef _WIN32
    return MoveFileExW(source.c_str(), destination.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    return !error;
#endif
}
}

ProfileManager::LoadStatus ProfileManager::load(const std::filesystem::path& path) {
    path_ = path;
    profiles_.clear();
    writable_ = false;
    loadStatus_ = LoadStatus::ReadError;

    if (path_.empty()) return loadStatus_;

    std::error_code error;
    const bool exists = std::filesystem::exists(path_, error);
    if (error) return loadStatus_;
    if (!exists) {
        writable_ = true;
        return loadStatus_ = LoadStatus::NotFound;
    }
    if (!std::filesystem::is_regular_file(path_, error) || error) return loadStatus_;

    std::ifstream file(path_, std::ios::binary);
    if (!file) return loadStatus_;

    std::string line;
    if (!std::getline(file, line)) {
        return loadStatus_ = file.bad() ? LoadStatus::ReadError : LoadStatus::InvalidFormat;
    }
    if (line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
    if (Utf8Util::trimAscii(line) != kHeader)
        return loadStatus_ = LoadStatus::InvalidFormat;

    std::string active;
    if (!std::getline(file, line) || !parseNamedLine(line, "ACTIVE", active))
        return loadStatus_ = file.bad() ? LoadStatus::ReadError : LoadStatus::InvalidFormat;

    std::vector<std::string> loaded;
    while (std::getline(file, line)) {
        if (Utf8Util::trimAscii(line).empty()) continue;
        std::string name;
        if (!parseNamedLine(line, "PROFILE", name) || !RecordManager::validName(name))
            return loadStatus_ = LoadStatus::InvalidFormat;
        if (std::find(loaded.begin(), loaded.end(), name) != loaded.end())
            return loadStatus_ = LoadStatus::InvalidFormat;
        loaded.push_back(std::move(name));
    }
    if (file.bad() || (!file.eof() && file.fail())) return loadStatus_;

    const bool emptyState = active.empty() && loaded.empty();
    const bool activeState = RecordManager::validName(active) && !loaded.empty() &&
                             loaded.front() == active;
    if (!emptyState && !activeState) return loadStatus_ = LoadStatus::InvalidFormat;

    profiles_.swap(loaded);
    writable_ = true;
    return loadStatus_ = LoadStatus::Ok;
}

ProfileManager::SelectStatus ProfileManager::selectOrCreate(const std::string& name) {
    if (!RecordManager::validName(name)) return SelectStatus::InvalidName;
    if (!writable_) return SelectStatus::Blocked;

    auto next = profiles_;
    const auto found = std::find(next.begin(), next.end(), name);
    const bool created = found == next.end();
    if (created) {
        next.insert(next.begin(), name);
    } else if (found != next.begin()) {
        std::string selected = std::move(*found);
        next.erase(found);
        next.insert(next.begin(), std::move(selected));
    } else {
        return SelectStatus::Selected;
    }

    if (!writeAll(next)) return SelectStatus::WriteError;

    profiles_.swap(next);
    loadStatus_ = LoadStatus::Ok;
    return created ? SelectStatus::Created : SelectStatus::Selected;
}

bool ProfileManager::initialize(const std::vector<std::string>& orderedProfiles) {
    if (!writable_ || !profiles_.empty() || orderedProfiles.empty()) return false;
    std::vector<std::string> checked;
    checked.reserve(orderedProfiles.size());
    for (const auto& name : orderedProfiles) {
        if (!RecordManager::validName(name) ||
            std::find(checked.begin(), checked.end(), name) != checked.end()) return false;
        checked.push_back(name);
    }
    if (!writeAll(checked)) return false;
    profiles_.swap(checked);
    loadStatus_ = LoadStatus::Ok;
    return true;
}

bool ProfileManager::writeAll(const std::vector<std::string>& profiles) {
    if (path_.empty() || profiles.empty()) return false;

    const auto temporary = unusedSibling(path_);
    if (temporary.empty()) return false;

    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return false;
    }

    file << kHeader << '\n';
    file << "ACTIVE " << std::quoted(profiles.front()) << '\n';
    for (const auto& name : profiles)
        file << "PROFILE " << std::quoted(name) << '\n';
    file.flush();
    file.close();

    std::error_code cleanupError;
    if (file.fail()) {
        std::filesystem::remove(temporary, cleanupError);
        return false;
    }
    if (!replaceFile(temporary, path_)) {
        std::filesystem::remove(temporary, cleanupError);
        return false;
    }
    return true;
}

const std::vector<std::string>& ProfileManager::profiles() const { return profiles_; }

const std::string& ProfileManager::activeProfile() const {
    static const std::string empty;
    return profiles_.empty() ? empty : profiles_.front();
}

bool ProfileManager::hasActiveProfile() const { return !profiles_.empty(); }
bool ProfileManager::canSave() const { return writable_; }
ProfileManager::LoadStatus ProfileManager::loadStatus() const { return loadStatus_; }
const std::filesystem::path& ProfileManager::path() const { return path_; }
