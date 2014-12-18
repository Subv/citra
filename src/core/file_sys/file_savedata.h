// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/file_backend.h"
#include "core/file_sys/archive_savedata.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

class File_SaveData final : public FileBackend {
public:
    File_SaveData();
    File_SaveData(const Archive_SaveData* archive, const Path& path, const Mode mode);
    ~File_SaveData() override;

    /**
     * Open the file
     * @return true if the file opened correctly
     */
    bool Open() override;

    /**
     * Read data from the file
     * @param offset Offset in bytes to start reading data from
     * @param length Length in bytes of data to read from file
     * @param buffer Buffer to read data into
     * @return Number of bytes read
     */
    size_t Read(const u64 offset, const u32 length, u8* buffer) const override;

    /**
     * Write data to the file
     * @param offset Offset in bytes to start writing data to
     * @param length Length in bytes of data to write to file
     * @param flush The flush parameters (0 == do not flush)
     * @param buffer Buffer to read data from
     * @return Number of bytes written
     */
    size_t Write(const u64 offset, const u32 length, const u32 flush, const u8* buffer) const override;

    /**
     * Get the size of the file in bytes
     * @return Size of the file in bytes
     */
    size_t GetSize() const override;

    /**
     * Set the size of the file in bytes
     * @param size New size of the file
     * @return true if successful
     */
    bool SetSize(const u64 size) const override;

    /**
     * Close the file
     * @return true if the file closed correctly
     */
    bool Close() const override;

private:
    std::string path;
    Mode mode;
    FileUtil::IOFile* file;
};

} // namespace FileSys
