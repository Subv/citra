// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/directory_savedata.h"
#include "core/file_sys/archive_savedata.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Directory_SaveData::Directory_SaveData(const Archive_SaveData* archive, const Path& path) {
    // TODO(Link Mauve): normalize path into an absolute path without "..", it can currently bypass
    // the root directory we set while opening the archive.
    // For example, opening /../../usr/bin can give the emulated program your installed programs.
    this->path = archive->GetMountPoint() + path.AsString();
}

Directory_SaveData::~Directory_SaveData() {
    Close();
}

bool Directory_SaveData::Open() {
    if (!FileUtil::IsDirectory(path))
        return false;
    FileUtil::ScanDirectoryTree(path, directory);
    children_iterator = directory.children.begin();
    return true;
}

u32 Directory_SaveData::Read(const u32 count, Entry* entries) {
    u32 entries_read = 0;

    while (entries_read < count && children_iterator != directory.children.cend()) {
        const FileUtil::FSTEntry& file = *children_iterator;
        const std::string& filename = file.virtualName;
        Entry& entry = entries[entries_read];

        LOG_TRACE(Service_FS, "File %s: size=%llu dir=%d", filename.c_str(), file.size, file.isDirectory);

        // TODO(Link Mauve): use a proper conversion to UTF-16.
        for (size_t j = 0; j < FILENAME_LENGTH; ++j) {
            entry.filename[j] = filename[j];
            if (!filename[j])
                break;
        }

        FileUtil::SplitFilename83(filename, entry.short_name, entry.extension);

        entry.is_directory = file.isDirectory;
        entry.is_hidden = (filename[0] == '.');
        entry.is_read_only = 0;
        entry.file_size = file.size;

        // We emulate a SD card where the archive bit has never been cleared, as it would be on
        // most user SD cards.
        // Some homebrews (blargSNES for instance) are known to mistakenly use the archive bit as a
        // file bit.
        entry.is_archive = !file.isDirectory;

        ++entries_read;
        ++children_iterator;
    }
    return entries_read;
}

bool Directory_SaveData::Close() const {
    return true;
}

} // namespace FileSys
