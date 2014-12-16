// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/disk_file.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

DiskFile::DiskFile(const DiskArchive* archive, const Path& path, const Mode mode) {
    // TODO(Link Mauve): normalize path into an absolute path without "..", it can currently bypass
    // the root directory we set while opening the archive.
    // For example, opening /../../etc/passwd can give the emulated program your users list.
    this->path = archive->GetMountPoint() + path.AsString();
    this->mode.hex = mode.hex;
    this->archive = archive;
}

DiskFile::~DiskFile() {
    Close();
}

bool DiskFile::Open() {
    if (!mode.create_flag && !FileUtil::Exists(path)) {
        LOG_ERROR(Service_FS, "Non-existing file %s can’t be open without mode create.", path.c_str());
        return false;
    }

    std::string mode_string;
    if (mode.create_flag)
        mode_string = "w+";
    else if (mode.write_flag)
        mode_string = "r+"; // Files opened with Write access can be read from
    else if (mode.read_flag)
        mode_string = "r";
    
    // Open the file in binary mode, to avoid problems with CR/LF on Windows systems
    mode_string += "b";

    file = new FileUtil::IOFile(path, mode_string.c_str());
    return true;
}

size_t DiskFile::Read(const u64 offset, const u32 length, u8* buffer) const {
    file->Seek(offset, SEEK_SET);
    return file->ReadBytes(buffer, length);
}

size_t DiskFile::Write(const u64 offset, const u32 length, const u32 flush, const u8* buffer) const {
    file->Seek(offset, SEEK_SET);
    size_t written = file->WriteBytes(buffer, length);
    if (flush)
        file->Flush();
    return written;
}

size_t DiskFile::GetSize() const {
    return static_cast<size_t>(file->GetSize());
}

bool DiskFile::SetSize(const u64 size) const {
    file->Resize(size);
    file->Flush();
    return true;
}

bool DiskFile::Close() const {
    return file->Close();
}

} // namespace FileSys
