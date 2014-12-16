// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/disk_archive.h"
#include "core/file_sys/disk_directory.h"
#include "core/file_sys/disk_file.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

DiskArchive::DiskArchive(const std::string& mount_point) {
    this->mount_point = mount_point;
}

DiskArchive::~DiskArchive() {
}

std::unique_ptr<FileBackend> DiskArchive::OpenFile(const Path& path, const Mode mode) const {
    LOG_DEBUG(Service_FS, "called path=%s mode=%u", path.DebugStr().c_str(), mode.hex);
    DiskFile* file = new DiskFile(this, path, mode);
    if (!file->Open())
        return nullptr;
    return std::unique_ptr<FileBackend>(file);
}

bool DiskArchive::DeleteFile(const FileSys::Path& path) const {
    return FileUtil::Delete(GetMountPoint() + path.AsString());
}

bool DiskArchive::RenameFile(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

bool DiskArchive::DeleteDirectory(const FileSys::Path& path) const {
    return FileUtil::DeleteDir(GetMountPoint() + path.AsString());
}

bool DiskArchive::CreateDirectory(const Path& path) const {
    return FileUtil::CreateDir(GetMountPoint() + path.AsString());
}

bool DiskArchive::RenameDirectory(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

std::unique_ptr<DirectoryBackend> DiskArchive::OpenDirectory(const Path& path) const {
    LOG_DEBUG(Service_FS, "called path=%s", path.DebugStr().c_str());
    DiskDirectory* directory = new DiskDirectory(this, path);
    if (!directory->Open())
        return nullptr;
    return std::unique_ptr<DirectoryBackend>(directory);
}

std::string DiskArchive::GetMountPoint() const {
    return mount_point;
}

} // namespace FileSys
