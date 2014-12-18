// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/directory_backend.h"
#include "core/file_sys/disk_archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

class DiskDirectory : public DirectoryBackend {
public:
    DiskDirectory();
    DiskDirectory(const DiskArchive* archive, const Path& path);
    ~DiskDirectory() override;

    /**
    * Open the directory
    * @return true if the directory opened correctly
    */
    bool Open() override;

    /**
     * List files contained in the directory
     * @param count Number of entries to return at once in entries
     * @param entries Buffer to read data into
     * @return Number of entries listed
     */
    u32 Read(const u32 count, Entry* entries) override;

    /**
     * Close the directory
     * @return true if the directory closed correctly
     */
    bool Close() const override;

protected:
    DiskArchive const* archive;
    std::string path;
    u32 total_entries_in_directory;
    FileUtil::FSTEntry directory;

    // We need to remember the last entry we returned, so a subsequent call to Read will continue
    // from the next one.  This iterator will always point to the next unread entry.
    std::vector<FileUtil::FSTEntry>::iterator children_iterator;
};

} // namespace FileSys
