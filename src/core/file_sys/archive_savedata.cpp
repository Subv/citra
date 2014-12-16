// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/file_savedata.h"
#include "core/file_sys/directory_savedata.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_SaveData::Archive_SaveData(const std::string& mount_point, u64 program_id) {
    this->mount_point = mount_point + std::to_string(program_id) + DIR_SEP;
    LOG_INFO(Service_FS, "Directory %s set as SaveData.", this->mount_point.c_str());
}

Archive_SaveData::CreateSaveDataResult Archive_SaveData::Initialize() {
    if (FileUtil::Exists(mount_point))
        return CreateSaveDataResult::AlreadyExists;

    if (!FileUtil::CreateFullPath(mount_point)) {
        LOG_ERROR(Service_FS, "Unable to create SaveData path.");
        return CreateSaveDataResult::Failure;
    }

    return CreateSaveDataResult::Success;
}

/**
 * Open a file specified by its path, using the specified mode
 * @param path Path relative to the archive
 * @param mode Mode to open the file with
 * @return Opened file, or nullptr
 */
std::unique_ptr<FileBackend> Archive_SaveData::OpenFile(const Path& path, const Mode mode) const {
    LOG_DEBUG(Service_FS, "called path=%s mode=%u", path.DebugStr().c_str(), mode.hex);
    File_SaveData* file = new File_SaveData(this, path, mode);
    if (!file->Open())
        return nullptr;
    return std::unique_ptr<FileBackend>(file);
}

/**
 * Delete a file specified by its path
 * @param path Path relative to the archive
 * @return Whether the file could be deleted
 */
bool Archive_SaveData::DeleteFile(const FileSys::Path& path) const {
    return FileUtil::Delete(GetMountPoint() + path.AsString());
}

bool Archive_SaveData::RenameFile(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

/**
 * Delete a directory specified by its path
 * @param path Path relative to the archive
 * @return Whether the directory could be deleted
 */
bool Archive_SaveData::DeleteDirectory(const FileSys::Path& path) const {
    return FileUtil::DeleteDir(GetMountPoint() + path.AsString());
}

/**
 * Create a directory specified by its path
 * @param path Path relative to the archive
 * @return Whether the directory could be created
 */
bool Archive_SaveData::CreateDirectory(const Path& path) const {
    return FileUtil::CreateDir(GetMountPoint() + path.AsString());
}

bool Archive_SaveData::RenameDirectory(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

/**
 * Open a directory specified by its path
 * @param path Path relative to the archive
 * @return Opened directory, or nullptr
 */
std::unique_ptr<DirectoryBackend> Archive_SaveData::OpenDirectory(const Path& path) const {
    LOG_DEBUG(Service_FS, "called path=%s", path.DebugStr().c_str());
    Directory_SaveData* directory = new Directory_SaveData(this, path);
    if (!directory->Open())
        return nullptr;
    return std::unique_ptr<DirectoryBackend>(directory);
}

/**
 * Getter for the path used for this Archive
 * @return Mount point of that passthrough archive
 */
std::string Archive_SaveData::GetMountPoint() const {
    return mount_point;
}

} // namespace FileSys
