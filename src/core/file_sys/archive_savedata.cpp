// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/disk_file.h"
#include "core/file_sys/disk_directory.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_SaveData::Archive_SaveData(const std::string& mount_point, u64 program_id) 
    : DiskArchive(mount_point + std::to_string(program_id) + DIR_SEP) {
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

} // namespace FileSys
