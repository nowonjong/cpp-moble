#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Stores local player names in most-recently-selected order. The first name is
// always the active profile.
class ProfileManager {
public:
    enum class LoadStatus { Ok, NotFound, InvalidFormat, ReadError };
    enum class SelectStatus { Selected, Created, InvalidName, Blocked, WriteError };

    LoadStatus load(const std::filesystem::path& path);
    bool initialize(const std::vector<std::string>& orderedProfiles);
    SelectStatus selectOrCreate(const std::string& name);

    const std::vector<std::string>& profiles() const;
    const std::string& activeProfile() const;
    bool hasActiveProfile() const;
    bool canSave() const;
    LoadStatus loadStatus() const;
    const std::filesystem::path& path() const;

private:
    bool writeAll(const std::vector<std::string>& profiles);

    std::filesystem::path path_;
    std::vector<std::string> profiles_;
    bool writable_ = false;
    LoadStatus loadStatus_ = LoadStatus::ReadError;
};
