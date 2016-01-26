// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/core.h"
#include "core/arm/arm_interface.h"
#include "core/hle/hle.h"
#include "core/hle/service/ldr_ro.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/vm_manager.h"
#include "common/file_util.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace LDR_RO

namespace LDR_RO {

struct SegmentTableEntry {
    u32 segment_offset;
    u32 segment_size;
    u32 segment_id;
};

struct Patch {
    u32 offset;
    u8 type;
    u8 unk;
    u8 unk2;
    u8 unk3;
    u32 x;

    u8 GetTargetSegment() { return offset & 0xF; }
    u32 GetSegmentOffset() { return offset >> 4; }
};

struct Unk3Patch {
    u32 segment_offset;
    u32 patches_offset;

    u8 GetTargetSegment() { return segment_offset & 0xF; }
    u32 GetSegmentOffset() { return segment_offset >> 4; }
};

struct Unk2TableEntry {
    u32 offset_or_index; ///< Index in the CRO's segment offset table (unk1) for table1 entries, or segment_offset for table2 entries
    u32 patches_offset;
};

struct Unk2Patch {
    u32 string_offset;
    u32 table1_offset;
    u32 table1_num;
    u32 table2_offset;
    u32 table2_num;

    Unk2TableEntry* GetTable1Entry(u32 index);
    Unk2TableEntry* GetTable2Entry(u32 index);
};

Unk2TableEntry* Unk2Patch::GetTable1Entry(u32 index) {
    return reinterpret_cast<Unk2TableEntry*>(Memory::GetPointer(table1_offset) + sizeof(Unk2TableEntry) * index);
}

Unk2TableEntry* Unk2Patch::GetTable2Entry(u32 index) {
    return reinterpret_cast<Unk2TableEntry*>(Memory::GetPointer(table2_offset) + sizeof(Unk2TableEntry) * index);
}

struct ExportTableEntry {
    u32 name_offset;
    u32 segment_offset;

    u8 GetTargetSegment() { return segment_offset & 0xF; }
    u32 GetSegmentOffset() { return segment_offset >> 4; }
};

struct ImportTableEntry {
    u32 name_offset;
    u32 symbol_offset;
};

struct ExportTreeEntry {
    u16 segment_offset;
    u16 next;
    u16 next_level;
    u16 export_table_id;

    u8 GetTargetSegment() { return segment_offset & 0x7; }
    u32 GetSegmentOffset() { return segment_offset >> 3; }
};

struct ExportedSymbol {
    std::string name;
    u32 cro_base;
    u32 cro_offset;
};

struct CROHeader {
    u8 sha2_hash[0x80];
    char magic[4];
    u32 name_offset;
    u32 next_cro;
    u32 previous_cro;
    u32 file_size;
    u32 unk_size1;
    u32 unk_address;
    INSERT_PADDING_WORDS(0x4);
    u32 segment_offset;
    u32 code_offset;
    u32 code_size;
    u32 unk_offset;
    u32 unk_size;
    u32 module_name_offset;
    u32 module_name_size;
    u32 segment_table_offset;
    u32 segment_table_num;
    u32 export_table_offset;
    u32 export_table_num;
    u32 unk1_offset;
    u32 unk1_size;
    u32 export_strings_offset;
    u32 export_strings_num;
    u32 export_tree_offset;
    u32 export_tree_num;
    u32 unk2_offset;
    u32 unk2_num;
    u32 import_patches_offset;
    u32 import_patches_num;
    u32 import_table1_offset;
    u32 import_table1_num;
    u32 import_table2_offset;
    u32 import_table2_num;
    u32 import_table3_offset;
    u32 import_table3_num;
    u32 import_strings_offset;
    u32 import_strings_num;
    u32 unk3_offset;
    u32 unk3_num;
    u32 relocation_patches_offset;
    u32 relocation_patches_num;
    u32 unk4_offset; /// More patches?
    u32 unk4_num;

    u8 GetImportPatchesTargetSegment() { return segment_offset & 0xF; }
    u32 GetImportPatchesSegmentOffset() { return segment_offset >> 4; }

    SegmentTableEntry GetSegmentTableEntry(u32 index) const;
    void SetSegmentTableEntry(u32 index, const SegmentTableEntry& entry);
    ResultCode RelocateSegmentsTable(u32 base, u32 size, u32 data_section0, u32 data_section1, u32& prev_data_section0);

    ExportTableEntry* GetExportTableEntry(u32 index);
    ResultCode RelocateExportsTable(u32 base);

    ExportTreeEntry* GetExportTreeEntry(u32 index);

    Patch* GetImportPatch(u32 index);

    ImportTableEntry* GetImportTable1Entry(u32 index);
    void RelocateImportTable1(u32 base);

    ImportTableEntry* GetImportTable2Entry(u32 index);
    void RelocateImportTable2(u32 base);

    ImportTableEntry* GetImportTable3Entry(u32 index);
    void RelocateImportTable3(u32 base);

    Unk3Patch* GetUnk3PatchEntry(u32 index);

    Patch* GetRelocationPatchEntry(u32 index);

    Unk2Patch* GetUnk2PatchEntry(u32 index);

    u32 GetUnk1TableEntry(u32 index);

    void RelocateUnk2Patches(u32 base);

    bool VerifyAndRelocateOffsets(u32 base, u32 size);
};

static std::unordered_map<std::string, ExportedSymbol> loaded_exports;
static std::vector<u32> loaded_cros;

SegmentTableEntry CROHeader::GetSegmentTableEntry(u32 index) const {
    SegmentTableEntry entry;
    memcpy(&entry, Memory::GetPointer(segment_table_offset + sizeof(SegmentTableEntry) * index), sizeof(SegmentTableEntry));
    return entry;
}

void CROHeader::SetSegmentTableEntry(u32 index, const SegmentTableEntry& entry) {
    memcpy(Memory::GetPointer(segment_table_offset + sizeof(SegmentTableEntry) * index), &entry, sizeof(SegmentTableEntry));
}

ResultCode CROHeader::RelocateSegmentsTable(u32 base, u32 size, u32 data_section0, u32 data_section1, u32& prev_data_section0) {
    u32 cro_end = base + size;

    prev_data_section0 = 0;
    for (int i = 0; i < segment_table_num; ++i) {
        SegmentTableEntry entry = GetSegmentTableEntry(i);
        if (entry.segment_id == 2) {
            prev_data_section0 = entry.segment_offset;
            entry.segment_offset = data_section0;
        } else if (entry.segment_id == 3) {
            entry.segment_offset = data_section1;
        } else if (entry.segment_offset) {
            entry.segment_offset += base;
            if (entry.segment_offset > cro_end)
                return ResultCode(0xD9012C19);
        }
        SetSegmentTableEntry(i, entry);
    }

    return RESULT_SUCCESS;
}

ExportTreeEntry* CROHeader::GetExportTreeEntry(u32 index) {
    return reinterpret_cast<ExportTreeEntry*>(Memory::GetPointer(export_tree_offset + sizeof(ExportTreeEntry) * index));
}

u32 CROHeader::GetUnk1TableEntry(u32 index) {
    return *reinterpret_cast<u32*>(Memory::GetPointer(unk1_offset + sizeof(u32) * index));
}

ExportTableEntry* CROHeader::GetExportTableEntry(u32 index) {
    return reinterpret_cast<ExportTableEntry*>(Memory::GetPointer(export_table_offset + sizeof(ExportTableEntry) * index));
}

ResultCode CROHeader::RelocateExportsTable(u32 base) {
    for (int i = 0; i < export_table_num; ++i) {
        ExportTableEntry* entry = GetExportTableEntry(i);
        if (entry->name_offset)
            entry->name_offset += base;

        if (entry->name_offset < export_strings_offset ||
            entry->name_offset > export_strings_offset + export_strings_num)
            return ResultCode(0xD9012C11);
    }

    return RESULT_SUCCESS;
}

Patch* CROHeader::GetImportPatch(u32 index) {
    return reinterpret_cast<Patch*>(Memory::GetPointer(import_patches_offset + sizeof(Patch) * index));
}

ImportTableEntry* CROHeader::GetImportTable1Entry(u32 index) {
    return reinterpret_cast<ImportTableEntry*>(Memory::GetPointer(import_table1_offset + sizeof(ImportTableEntry) * index));
}

ImportTableEntry* CROHeader::GetImportTable2Entry(u32 index) {
    return reinterpret_cast<ImportTableEntry*>(Memory::GetPointer(import_table2_offset + sizeof(ImportTableEntry) * index));
}

ImportTableEntry* CROHeader::GetImportTable3Entry(u32 index) {
    return reinterpret_cast<ImportTableEntry*>(Memory::GetPointer(import_table3_offset + sizeof(ImportTableEntry) * index));
}

void CROHeader::RelocateImportTable1(u32 base) {
    for (int i = 0; i < import_table1_num; ++i) {
        ImportTableEntry* entry = GetImportTable1Entry(i);
        if (entry->name_offset)
            entry->name_offset += base;
        if (entry->symbol_offset)
            entry->symbol_offset += base;
    }
}

void CROHeader::RelocateImportTable2(u32 base) {
    for (int i = 0; i < import_table2_num; ++i) {
        ImportTableEntry* entry = GetImportTable2Entry(i);
        if (entry->symbol_offset)
            entry->symbol_offset += base;
    }
}

void CROHeader::RelocateImportTable3(u32 base) {
    for (int i = 0; i < import_table3_num; ++i) {
        ImportTableEntry* entry = GetImportTable3Entry(i);
        if (entry->symbol_offset)
            entry->symbol_offset += base;
    }
}

void CROHeader::RelocateUnk2Patches(u32 base) {
    for (int i = 0; i < unk2_num; ++i) {
        Unk2Patch* entry = GetUnk2PatchEntry(i);
        if (entry->string_offset)
            entry->string_offset += base;
        if (entry->table1_offset)
            entry->table1_offset += base;
        if (entry->table2_offset)
            entry->table2_offset += base;
    }
}

Unk3Patch* CROHeader::GetUnk3PatchEntry(u32 index) {
    return reinterpret_cast<Unk3Patch*>(Memory::GetPointer(unk3_offset * sizeof(Unk3Patch) * index));
}

Patch* CROHeader::GetRelocationPatchEntry(u32 index) {
    return reinterpret_cast<Patch*>(Memory::GetPointer(relocation_patches_offset + sizeof(Patch) * index));
}

Unk2Patch* CROHeader::GetUnk2PatchEntry(u32 index) {
    return reinterpret_cast<Unk2Patch*>(Memory::GetPointer(unk2_offset + sizeof(Unk2Patch) * index));
}

bool CROHeader::VerifyAndRelocateOffsets(u32 base, u32 size) {
    u32 end = base + size;

    // Error if the magic is invalid
    if (memcmp(magic, "CRO0", 4) != 0)
        return false;

    // If these values are set the game might be trying to load the same CRO multiple times
    if (next_cro || previous_cro)
        return false;

    // This seems to be a hard limit set by the RO module
    if (file_size >= 0x10000000 || unk_size1 >= 0x10000000)
        return false;

    if (unk_address != 0)
        return false;

    // Hard limit set by the RO module, it is unknown what the value means
    if (code_offset < 0x138)
        return false;

    if (module_name_offset < code_offset)
        return false;

    if (module_name_offset > segment_table_offset)
        return false;

    if (export_table_offset < segment_table_offset)
        return false;

    if (export_table_offset > export_tree_offset)
        return false;

    if (unk1_offset < export_tree_offset)
        return false;

    if (unk1_offset > export_strings_offset)
        return false;

    if (unk2_offset < export_strings_offset)
        return false;

    if (unk2_offset > import_patches_offset)
        return false;

    if (import_table1_offset < import_patches_offset)
        return false;

    if (import_table1_offset > import_table2_offset)
        return false;

    if (import_table3_offset < import_table2_offset)
        return false;

    if (import_table3_offset > import_strings_offset)
        return false;

    if (unk3_offset < import_strings_offset)
        return false;

    if (unk3_offset > relocation_patches_offset)
        return false;

    if (unk4_offset < relocation_patches_offset)
        return false;

    if (unk4_offset > unk_offset)
        return false;

    if (unk_offset > file_size)
        return false;

    if (name_offset) {
        name_offset += base;
        if (name_offset > end)
            return false;
    }

    if (code_offset) {
        code_offset += base;
        if (code_offset > end)
            return false;
    }

    if (unk_offset) {
        unk_offset += base;
        if (unk_offset > end)
            return false;
    }

    if (module_name_offset) {
        module_name_offset += base;
        if (module_name_offset > end)
            return false;
    }

    if (segment_table_offset) {
        segment_table_offset += base;
        if (segment_table_offset > end)
            return false;
    }

    if (export_table_offset) {
        export_table_offset += base;
        if (export_table_offset > end)
            return false;
    }

    if (unk1_offset) {
        unk1_offset += base;
        if (unk1_offset > end)
            return false;
    }

    if (export_strings_offset) {
        export_strings_offset += base;
        if (export_strings_offset > end)
            return false;
    }

    if (export_tree_offset) {
        export_tree_offset += base;
        if (export_tree_offset > end)
            return false;
    }

    if (unk2_offset) {
        unk2_offset += base;
        if (unk2_offset > end)
            return false;
    }

    if (import_patches_offset) {
        import_patches_offset += base;
        if (import_patches_offset > end)
            return false;
    }

    if (import_table1_offset) {
        import_table1_offset += base;
        if (import_table1_offset > end)
            return false;
    }

    if (import_table2_offset) {
        import_table2_offset += base;
        if (import_table2_offset > end)
            return false;
    }

    if (import_table3_offset) {
        import_table3_offset += base;
        if (import_table3_offset > end)
            return false;
    }

    if (import_strings_offset) {
        import_strings_offset += base;
        if (import_strings_offset > end)
            return false;
    }

    if (unk3_offset) {
        unk3_offset += base;
        if (unk3_offset > end)
            return false;
    }

    if (relocation_patches_offset) {
        relocation_patches_offset += base;
        if (relocation_patches_offset > end)
            return false;
    }

    if (unk4_offset) {
        unk4_offset += base;
        if (unk4_offset > end)
            return false;
    }

    if (code_offset + code_size > end ||
        unk_offset + unk_size > end ||
        module_name_offset + module_name_size > end ||
        segment_table_offset + sizeof(SegmentTableEntry) * segment_table_num > end ||
        export_table_offset + sizeof(ExportTableEntry) * export_table_num > end ||
        unk1_offset + sizeof(u32) * unk1_size > end ||
        export_strings_offset + export_strings_num > end ||
        export_tree_offset + sizeof(ExportTreeEntry) * export_tree_num > end ||
        unk2_offset + sizeof(Unk2Patch) * unk2_num > end ||
        import_patches_offset + sizeof(Patch) * import_patches_num > end ||
        import_table1_offset + sizeof(ImportTableEntry) * import_table1_num > end ||
        import_table2_offset + sizeof(ImportTableEntry) * import_table2_num > end ||
        import_table3_offset + sizeof(ImportTableEntry) * import_table3_num > end ||
        import_strings_offset + import_strings_num > end ||
        unk3_offset + sizeof(Unk3Patch) * unk3_num > end ||
        relocation_patches_offset + sizeof(Patch) * relocation_patches_num > end ||
        unk4_offset + 12 * unk4_num > end) {
            return false;
    }

    return true;
}

static void ApplyPatch(Patch* patch, u32 patch_base, u32 patch_address, u32* patch_address1 = nullptr) {
    if (!patch_address1)
        patch_address1 = &patch_address;

    if (patch_address == 0x119C74)
        __debugbreak();

    switch (patch->type) {
        case 2:
            Memory::Write32(patch_address, patch_base + patch->x);
            break;
        case 3:
            Memory::Write32(patch_address, patch_base + patch->x - *patch_address1);
            break;
        default:
            LOG_CRITICAL(Service_APT, "Unknown patch type %u", patch->type);
    }
}

static void ApplyImportPatches(CROHeader& header, u32 base) {
    u32 patch_base = 0;

    if (header.GetImportPatchesTargetSegment() < header.segment_table_num) {
        SegmentTableEntry base_segment = header.GetSegmentTableEntry(header.GetImportPatchesTargetSegment());
        patch_base = base_segment.segment_offset + header.GetImportPatchesSegmentOffset();
    }

    u32 v10 = 1;
    for (int i = 0; i < header.import_patches_num; ++i) {
        Patch* patch = header.GetImportPatch(i);
        SegmentTableEntry target_segment = header.GetSegmentTableEntry(patch->GetTargetSegment());
        ApplyPatch(patch, patch_base, target_segment.segment_offset + patch->GetSegmentOffset());
        if (v10)
            patch->unk2 = 0;
        v10 = patch->unk;
    }
}

static void ApplyListPatches(CROHeader& header, Patch* first_patch, u32 patch_base) {
    Patch* current_patch = first_patch;

    while (current_patch) {
        SegmentTableEntry target_segment = header.GetSegmentTableEntry(current_patch->GetTargetSegment());
        ApplyPatch(current_patch, patch_base, target_segment.segment_offset + current_patch->GetSegmentOffset());

        if (current_patch->unk)
            break;
        ++current_patch;
    }

    first_patch->unk2 = 1;
}

static void ApplyUnk3Patches(CROHeader& header, CROHeader* crs, u32 base) {
    for (int i = 0; i < header.unk3_num; ++i) {
        Unk3Patch* patch = header.GetUnk3PatchEntry(i);
        SegmentTableEntry segment = header.GetSegmentTableEntry(patch->GetTargetSegment());
        u32 patch_base = segment.segment_offset + patch->GetSegmentOffset();
        u32 patches_table = base + patch->patches_offset;

        Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(patches_table));
        ApplyListPatches(*crs, first_patch, patch_base);
    }
}

static void ApplyRelocationPatches(CROHeader& header, u32 base, u32 section0) {
    for (int i = 0; i < header.relocation_patches_num; ++i) {
        Patch* patch = header.GetRelocationPatchEntry(i);
        u32 segment_id = patch->GetTargetSegment();
        SegmentTableEntry target_segment = header.GetSegmentTableEntry(segment_id);
        u32 target_segment_offset = target_segment.segment_offset;

        if (segment_id == 2)
            target_segment_offset = section0;

        SegmentTableEntry base_segment = header.GetSegmentTableEntry(patch->unk);

        u32 patch_address = target_segment_offset + patch->GetSegmentOffset();
        u32 patch_address1 = target_segment.segment_offset + patch->GetSegmentOffset();

        ApplyPatch(patch, base_segment.segment_offset, patch_address, &patch_address1);
    }
}

static const u32 EXPORT_TABLE_FINISHED = 0x8000;

static u32 FindExportByName(CROHeader& header, char* str) {
    if (header.export_tree_num) {
        ExportTreeEntry* first_entry = header.GetExportTreeEntry(0);
        u32 len = strlen(str);
        ExportTreeEntry* next_entry = header.GetExportTreeEntry(first_entry->next);
        bool run = false;
        while (!run) {
            u16 next_offset = 0;
            u32 next_len = next_entry->GetSegmentOffset();
            u8 next_segment = str[next_len];
            if (next_len >= len)
                next_offset = next_entry->next;
            else if (!((next_segment >> next_entry->GetTargetSegment()) & 1))
                next_offset = next_entry->next;
            else
                next_offset = next_entry->next_level;
            run = next_offset & EXPORT_TABLE_FINISHED;
            next_entry = header.GetExportTreeEntry(next_offset & ~EXPORT_TABLE_FINISHED);
        }

        u32 export_id = next_entry->export_table_id;
        ExportTableEntry* export_entry = header.GetExportTableEntry(export_id);
        char* export_name = (char*)Memory::GetPointer(export_entry->name_offset);
        if (!strcmp(export_name, str)) {
            SegmentTableEntry segment = header.GetSegmentTableEntry(export_entry->GetTargetSegment());
            return segment.segment_offset + export_entry->GetSegmentOffset();
        }
    }
    return 0;
}

static void ApplyExitPatches(CROHeader& header, CROHeader* crs, u32 base) {
    // Find the "__aeabi_atexit" in the import table 1
    for (int i = 0; i < header.import_table1_num; ++i) {
        ImportTableEntry* entry = header.GetImportTable1Entry(i);
        // The name is already relocated
        char* entry_name = reinterpret_cast<char*>(Memory::GetPointer(entry->name_offset));
        if (!strcmp(entry_name, "__aeabi_atexit")) {
            // Only apply these patches if the CRS exports "nnroAeabiAtexit_"
            u32 offset = FindExportByName(*crs, "nnroAeabiAtexit_");

            ASSERT_MSG(offset, "Could not find nnroAeabiAtexit_ in the CRS");

            // Patch it!
            Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
            ApplyListPatches(header, first_patch, offset);
            return;
        }
    }
}

static void BackApplyImportTable1Patches(CROHeader& old_header, CROHeader& new_header) {
    for (int i = 0; i < old_header.import_table1_num; ++i) {
        ImportTableEntry* entry = old_header.GetImportTable1Entry(i);
        Patch* patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
        if (!patch->unk2) {
            u32 patch_base = FindExportByName(new_header, (char*)Memory::GetPointer(entry->name_offset));

            if (patch_base) {
                Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
                ApplyListPatches(old_header, first_patch, patch_base);
            }
        }
    }
}

static void ApplyImportTable1Patches(CROHeader& header, CROHeader* head) {
    for (int i = 0; i < header.import_table1_num; ++i) {
        ImportTableEntry* entry = header.GetImportTable1Entry(i);
        Patch* patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
        if (!patch->unk2) {
            u32 patch_base = 0;
            while (head) {
                patch_base = FindExportByName(*head, (char*)Memory::GetPointer(entry->name_offset));
                if (patch_base)
                    break;
                head = reinterpret_cast<CROHeader*>(Memory::GetPointer(head->next_cro));
            }
            if (patch_base) {
                Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
                ApplyListPatches(header, first_patch, patch_base);
            }
        }
    }
}

static u32 GetCROBaseByName(char* name) {
    for (u32 base : loaded_cros) {
        CROHeader* header = reinterpret_cast<CROHeader*>(Memory::GetPointer(base));
        char* cro_name = reinterpret_cast<char*>(Memory::GetPointer(header->name_offset));

        if (!strcmp(cro_name, name))
            return base;
    }
    return 0;
}

static void ApplyUnk2Patches(CROHeader& header, u32 base) {
    for (int i = 0; i < header.unk2_num; ++i) {
        Unk2Patch* entry = header.GetUnk2PatchEntry(i);
        u32 cro_base = GetCROBaseByName(reinterpret_cast<char*>(Memory::GetPointer(entry->string_offset)));
        if (cro_base == 0)
            continue;

        CROHeader* patch_cro = reinterpret_cast<CROHeader*>(Memory::GetPointer(cro_base));
        // Apply the patches from the first table
        for (int j = 0; j < entry->table1_num; ++j) {
            Unk2TableEntry* table1_entry = entry->GetTable1Entry(j);
            u32 unk1_table_entry = patch_cro->GetUnk1TableEntry(table1_entry->offset_or_index);
            u32 base_segment_id = unk1_table_entry & 0xF;
            u32 base_segment_offset = unk1_table_entry >> 4;
            SegmentTableEntry base_segment = patch_cro->GetSegmentTableEntry(base_segment_id);

            Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(table1_entry->patches_offset));
            ApplyListPatches(header, first_patch, base_segment.segment_offset + base_segment_offset);
        }

        // Apply the patches from the second table
        for (int j = 0; j < entry->table2_num; ++j) {
            Unk2TableEntry* table2_entry = entry->GetTable2Entry(j);
            u32 base_segment_id = table2_entry->offset_or_index & 0xF;
            u32 base_segment_offset = table2_entry->offset_or_index >> 4;
            SegmentTableEntry base_segment = patch_cro->GetSegmentTableEntry(base_segment_id);

            Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(table2_entry->patches_offset));
            ApplyListPatches(header, first_patch, base_segment.segment_offset + base_segment_offset);
        }
    }
}

static void BackApplyUnk2Patches(CROHeader& header, u32 base, CROHeader& patch_cro, u32 new_base) {
    for (int i = 0; i < header.unk2_num; ++i) {
        Unk2Patch* entry = header.GetUnk2PatchEntry(i);
        char* old_cro_name = reinterpret_cast<char*>(Memory::GetPointer(entry->string_offset));
        char* new_cro_name = reinterpret_cast<char*>(Memory::GetPointer(patch_cro.name_offset));
        if (strcmp(old_cro_name, new_cro_name) != 0)
            continue;

        // Apply the patches from the first table
        for (int j = 0; j < entry->table1_num; ++j) {
            Unk2TableEntry* table1_entry = entry->GetTable1Entry(j);
            u32 unk1_table_entry = patch_cro.GetUnk1TableEntry(table1_entry->offset_or_index);
            u32 base_segment_id = unk1_table_entry & 0xF;
            u32 base_segment_offset = unk1_table_entry >> 4;
            SegmentTableEntry base_segment = patch_cro.GetSegmentTableEntry(base_segment_id);

            Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(table1_entry->patches_offset));
            ApplyListPatches(header, first_patch, base_segment.segment_offset + base_segment_offset);
        }

        // Apply the patches from the second table
        for (int j = 0; j < entry->table2_num; ++j) {
            Unk2TableEntry* table2_entry = entry->GetTable2Entry(j);
            u32 base_segment_id = table2_entry->offset_or_index & 0xF;
            u32 base_segment_offset = table2_entry->offset_or_index >> 4;
            SegmentTableEntry base_segment = patch_cro.GetSegmentTableEntry(base_segment_id);

            Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(table2_entry->patches_offset));
            ApplyListPatches(header, first_patch, base_segment.segment_offset + base_segment_offset);
        }
    }
}

static void LoadExportsTable(CROHeader& header, u32 base) {
    for (int i = 0; i < header.export_table_num; ++i) {
        ExportTableEntry* entry = header.GetExportTableEntry(i);
        SegmentTableEntry target_segment = header.GetSegmentTableEntry(entry->GetTargetSegment());
        ExportedSymbol export_;
        export_.cro_base = base;
        export_.cro_offset = target_segment.segment_offset + entry->GetSegmentOffset();
        export_.name = reinterpret_cast<char*>(Memory::GetPointer(entry->name_offset));
        loaded_exports[export_.name] = export_;
    }
}

static void LinkCROs(CROHeader* crs, CROHeader& new_cro, u32 base) {
    if (crs->next_cro) {
        auto v3 = reinterpret_cast<CROHeader*>(Memory::GetPointer(crs->next_cro));
        crs = reinterpret_cast<CROHeader*>(Memory::GetPointer(v3->previous_cro));
        new_cro.previous_cro = v3->previous_cro;
        new_cro.next_cro = 0;
        v3->previous_cro = base;
    } else {
        new_cro.next_cro = 0;
        new_cro.previous_cro = base;
    }

    crs->next_cro = base;
}

static ResultCode LoadCRO(u32 base, u32 size, CROHeader& header, u32 data_section0, u32 data_section1, bool is_crs) {
    // Relocate all offsets
    if (!header.VerifyAndRelocateOffsets(base, size))
        return ResultCode(0xD9012C11);

    if (header.module_name_size &&
        Memory::Read8(header.module_name_offset + header.module_name_size - 1) != 0) {
        // The module name must end with '\0'
        return ResultCode(0xD9012C0B);
    }

    u32 prev_section0 = 0;
    ResultCode result = RESULT_SUCCESS;

    if (!is_crs) {
        // Relocate segments
        result = header.RelocateSegmentsTable(base, size, data_section0, data_section1, prev_section0);
        if (result.IsError())
            return result;
    }

    // Rebase export table
    result = header.RelocateExportsTable(base);

    if (result.IsError())
        return result;

    if (header.export_strings_num &&
        Memory::Read8(header.export_strings_offset + header.export_strings_num - 1) != 0)
        return ResultCode(0xD9012C0B);

    CROHeader* crs = nullptr;
    if (is_crs)
        crs = &header;
    else
        crs = reinterpret_cast<CROHeader*>(Memory::GetPointer(loaded_cros.front()));

    // Rebase unk2
    header.RelocateUnk2Patches(base);

    // Apply import patches
    ApplyImportPatches(header, base);

    // Rebase import table 1 name & symbol offsets
    header.RelocateImportTable1(base);

    // Rebase import tables 2 & 3 symbol offsets
    header.RelocateImportTable2(base);
    header.RelocateImportTable3(base);

    if (!is_crs) {
        // Apply unk3 patches
        ApplyUnk3Patches(header, crs, base);
    }

    // Apply relocation patches
    ApplyRelocationPatches(header, base, prev_section0 + base);

    // Apply import table 1 patches
    ApplyExitPatches(header, crs, base);

    // Import Table 1
    ApplyImportTable1Patches(header, crs);

    // Load exports
    LoadExportsTable(header, base);

    if (!is_crs) {
        // Apply unk2 patches
        ApplyUnk2Patches(header, base);

        // Retroactively apply import table 1 patches to the previous CROs
        // Retroactively apply unk2 patches to the previous CROs
        for (auto itr = loaded_cros.rbegin(); itr != loaded_cros.rend(); ++itr) {
            u32 cro_base = *itr;
            CROHeader* cro_header = reinterpret_cast<CROHeader*>(Memory::GetPointer(cro_base));
            BackApplyImportTable1Patches(*cro_header, header);
            BackApplyUnk2Patches(*cro_header, cro_base, header, base);
        }
    }

    if (!is_crs) {
        // Link the CROs
        LinkCROs(reinterpret_cast<CROHeader*>(Memory::GetPointer(loaded_cros.front())), header, base);
    }

    loaded_cros.push_back(base);

    std::string module_name = Memory::GetString(header.module_name_offset, header.module_name_size);
    LOG_WARNING(Service_LDR, "Loaded CRO name %s", module_name.c_str());

    // Clear the instruction cache
    Core::g_app_core->ClearInstructionCache();

    return RESULT_SUCCESS;
}

/**
 * LDR_RO::Initialize service function
 *  Inputs:
 *      1 : CRS buffer pointer
 *      2 : CRS Size
 *      3 : Process memory address where the CRS will be mapped
 *      4 : Value, must be zero
 *      5 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u8* crs_buffer_ptr = Memory::GetPointer(cmd_buff[1]);
    u32 crs_size       = cmd_buff[2];
    u32 address        = cmd_buff[3];
    u32 value          = cmd_buff[4];
    u32 process        = cmd_buff[5];

    if (value != 0) {
        LOG_WARNING(Service_LDR, "This value should be zero, but is actually %u!", value);
    }

    LOG_WARNING(Service_LDR, "(STUBBED) called. crs_buffer_ptr=0x%08X, crs_size=0x%08X, address=0x%08X, value=0x%08X, process=0x%08X",
                crs_buffer_ptr, crs_size, address, value, process);

    loaded_exports.clear();
    loaded_cros.clear();

    std::shared_ptr<std::vector<u8>> cro = std::make_shared<std::vector<u8>>(crs_size);
    memcpy(cro->data(), crs_buffer_ptr, crs_size);

    // TODO(Subv): Check what the real hardware returns for MemoryState
    auto map_result = Kernel::g_current_process->vm_manager.MapMemoryBlock(address, cro, 0, crs_size, Kernel::MemoryState::Code);

    cmd_buff[0] = IPC::MakeHeader(1, 1, 0);

    if (map_result.Failed()) {
        LOG_ERROR(Service_LDR, "Error mapping memory block %08X", map_result.Code().raw);
        cmd_buff[1] = map_result.Code().raw;
        return;
    }

    CROHeader header;
    memcpy(&header, Memory::GetPointer(address), sizeof(CROHeader));

    ResultCode result = LoadCRO(address, crs_size, header, 0, 0, true);
    cmd_buff[1] = result.raw;

    if (result != RESULT_SUCCESS) {
        LOG_ERROR(Service_LDR, "Error loading CRS %08X", result.raw);
        return;
    }

    memcpy(Memory::GetPointer(address), &header, sizeof(CROHeader));
}

/**
 * LDR_RO::LoadCRR service function
 *  Inputs:
 *      1 : CRR buffer pointer
 *      2 : CRR Size
 *      3 : Value, must be zero
 *      4 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void LoadCRR(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 crs_buffer_ptr = cmd_buff[1];
    u32 crs_size       = cmd_buff[2];
    u32 value          = cmd_buff[3];
    u32 process        = cmd_buff[4];

    if (value != 0) {
        LOG_WARNING(Service_LDR, "This value should be zero, but is actually %u!", value);
    }

    cmd_buff[0] = IPC::MakeHeader(2, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_LDR, "(STUBBED) called. crs_buffer_ptr=0x%08X, crs_size=0x%08X, value=0x%08X, process=0x%08X",
                crs_buffer_ptr, crs_size, value, process);
}

struct UnknownStructure {
    u32 unk0;
    u32 unk1;
    u32 unk2;
    u32 unk3;
    u32 unk4;
};

static UnknownStructure GetStructure(CROHeader& cro, u32 fix_level) {
    u32 v2 = cro.code_offset + cro.code_size;

    if (v2 <= 0x138)
        v2 = 0x138;

    v2 = std::max<u32>(v2, cro.module_name_offset + cro.module_name_size);
    v2 = std::max<u32>(v2, cro.segment_table_offset + sizeof(SegmentTableEntry) * cro.segment_table_num);

    u32 v4 = v2;

    v2 = std::max<u32>(v2, cro.export_table_offset + sizeof(ExportTableEntry) * cro.export_table_num);
    v2 = std::max<u32>(v2, cro.unk1_offset + cro.unk1_size);
    v2 = std::max<u32>(v2, cro.export_strings_offset + cro.export_strings_num);
    v2 = std::max<u32>(v2, cro.export_tree_offset + sizeof(ExportTreeEntry) * cro.export_tree_num);

    u32 v7 = v2;

    v2 = std::max<u32>(v2, cro.unk2_offset + sizeof(Unk2Patch) * cro.unk2_offset);
    v2 = std::max<u32>(v2, cro.import_patches_offset + sizeof(Patch) * cro.import_patches_num);
    v2 = std::max<u32>(v2, cro.import_table1_offset + sizeof(ImportTableEntry) * cro.import_table1_num);
    v2 = std::max<u32>(v2, cro.import_table2_offset + sizeof(ImportTableEntry) * cro.import_table2_num);
    v2 = std::max<u32>(v2, cro.import_table3_offset + sizeof(ImportTableEntry) * cro.import_table3_num);
    v2 = std::max<u32>(v2, cro.import_strings_offset + cro.import_strings_num);

    u32 v12 = v2;

    v2 = std::max<u32>(v2, cro.unk4_offset + 12 * cro.unk4_num);
    v2 = std::max<u32>(v2, cro.unk3_offset + sizeof(Unk3Patch) * cro.unk3_num);
    v2 = std::max<u32>(v2, cro.relocation_patches_offset + sizeof(Patch) * cro.relocation_patches_num);

    UnknownStructure ret;
    ret.unk0 = v2;
    ret.unk1 = v12;
    ret.unk2 = v7;
    ret.unk3 = v4;
    ret.unk4 = 0;
    return ret;
}

static void LoadExeCRO(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u8* cro_buffer = Memory::GetPointer(cmd_buff[1]);
    u32 address = cmd_buff[2];
    u32 size = cmd_buff[3];

    u32 level = cmd_buff[10];

    bool link = cmd_buff[9] & 0xFF;

    ASSERT_MSG(link, "Link must be set");

    std::shared_ptr<std::vector<u8>> cro = std::make_shared<std::vector<u8>>(size);
    memcpy(cro->data(), cro_buffer, size);

    // TODO(Subv): Check what the real hardware returns for MemoryState
    auto map_result = Kernel::g_current_process->vm_manager.MapMemoryBlock(address, cro, 0, size, Kernel::MemoryState::Code);

    cmd_buff[0] = IPC::MakeHeader(4, 2, 0);

    if (map_result.Failed()) {
        LOG_CRITICAL(Service_LDR, "Error when mapping memory: %08X", map_result.Code().raw);
        cmd_buff[1] = map_result.Code().raw;
        return;
    }

    CROHeader header;
    memcpy(&header, Memory::GetPointer(address), sizeof(CROHeader));

    ResultCode result = LoadCRO(address, size, header, cmd_buff[4], cmd_buff[7], false);
    cmd_buff[1] = result.raw;

    if (result.IsError()) {
        LOG_CRITICAL(Service_LDR, "Error when loading CRO %08X", result.raw);
        return;
    }

    cmd_buff[2] = 0;

    auto struc = GetStructure(header, level);
    u32 value = struc.unk0;

    switch (level) {
    case 1:
        value = struc.unk1;
        break;
    case 2:
        value = struc.unk2;
        break;
    case 3:
        value = struc.unk3;
        break;
    default:
        break;
    }

    memcpy(header.magic, "FIXD", 4);
    header.unk3_offset = value;
    header.unk3_num = 0;
    header.relocation_patches_offset = value;
    header.relocation_patches_num = 0;
    header.unk4_offset = value;
    header.unk4_num = 0;

    if (level >= 2) {
        header.unk2_offset = value;
        header.unk2_num = 0;
        header.import_patches_offset = value;
        header.import_patches_num = 0;
        header.import_table1_offset = value;
        header.import_table1_num = 0;
        header.import_table2_offset = value;
        header.import_table2_num = 0;
        header.import_table3_offset = value;
        header.import_table3_num = 0;
        header.import_strings_offset = value;
        header.import_strings_num = 0;

        if (level >= 3) {
            header.export_table_offset = value;
            header.export_table_num = 0;
            header.unk1_offset = value;
            header.unk1_size = 0;
            header.export_strings_offset = value;
            header.export_strings_num = 0;
            header.export_tree_offset = value;
            header.export_tree_num = 0;
        }
    }

    u32 changed = (value + 0xFFF) >> 12 << 12;
    u32 cro_end = address + size;
    u32 v24 = cro_end - changed;
    cmd_buff[2] = size - v24;
    header.unk_address = size - v24;

    memcpy(Memory::GetPointer(address), &header, sizeof(CROHeader));

    LOG_WARNING(Service_LDR, "Loading CRO address=%08X level=%08X", address, level);
}

static void UnlinkCRO(CROHeader* crs, CROHeader* cro, u32 address) {
    auto v5_base = crs->previous_cro;

    if (v5_base == address) {
        auto v5 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v5_base));
        auto v7_base = v5->next_cro;
        if (v7_base) {
            auto v7 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v7_base));
            v7->previous_cro = v5->previous_cro;
        }
        crs->previous_cro = v7_base;
    } else {
        auto v8_base = crs->next_cro;
        auto v8 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v8_base));
        if (v8_base == address) {
            auto v8 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v8_base));
            auto v9_base = v8->next_cro;
            if (v9_base) {
                auto v9 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v9_base));
                v9->previous_cro = v8->previous_cro;
            }
            crs->next_cro = v9_base;
        } else {
            auto v10_base = cro->next_cro;
            if (v10_base) {
                auto v11_base = cro->previous_cro;
                auto v11 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v11_base));
                auto v10 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v10_base));
                v11->next_cro = v10_base;
                v10->previous_cro = v11_base;
            } else {
                auto v16_base = cro->previous_cro;
                auto v16 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v16_base));
                if (v8_base && v8->previous_cro == address) {
                    v8->previous_cro = v16_base;
                } else {
                    auto v5 = reinterpret_cast<CROHeader*>(Memory::GetPointer(v5_base));
                    v5->previous_cro = v16_base;
                }
                v16->next_cro = 0;
            }
        }
    }

    cro->previous_cro = 0;
    cro->next_cro = 0;
}

static void UnloadImportTablePatches(CROHeader* cro, Patch* first_patch, u32 base_offset) {
    Patch* patch = first_patch;
    while (patch) {
        SegmentTableEntry target_segment = cro->GetSegmentTableEntry(patch->GetTargetSegment());
        ApplyPatch(patch, base_offset, target_segment.segment_offset + patch->GetSegmentOffset());

        if (patch->unk)
            break;

        patch++;
    }

    first_patch->unk2 = 0;
}

static u32 CalculateBaseOffset(CROHeader* cro) {
    u32 base_offset = 0;

    if (cro->GetImportPatchesTargetSegment() < cro->segment_table_num) {
        SegmentTableEntry base_segment = cro->GetSegmentTableEntry(cro->GetImportPatchesTargetSegment());
        if (cro->GetImportPatchesSegmentOffset() < base_segment.segment_size)
            base_offset = base_segment.segment_offset + cro->GetImportPatchesSegmentOffset();
    }

    return base_offset;
}

static void UnloadImportTable1Patches(CROHeader* cro, u32 base_offset) {
    for (int i = 0; i < cro->import_table1_num; ++i) {
        ImportTableEntry* entry = cro->GetImportTable1Entry(i);
        Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
        UnloadImportTablePatches(cro, first_patch, base_offset);
    }
}

static void UnloadImportTable2Patches(CROHeader* cro, u32 base_offset) {
    for (int i = 0; i < cro->import_table2_num; ++i) {
        ImportTableEntry* entry = cro->GetImportTable2Entry(i);
        Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
        UnloadImportTablePatches(cro, first_patch, base_offset);
    }
}

static void UnloadImportTable3Patches(CROHeader* cro, u32 base_offset) {
    for (int i = 0; i < cro->import_table3_num; ++i) {
        ImportTableEntry* entry = cro->GetImportTable3Entry(i);
        Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
        UnloadImportTablePatches(cro, first_patch, base_offset);
    }
}

static void ApplyCRSImportTable1UnloadPatches(CROHeader* crs, CROHeader& unload, u32 base_offset) {
    for (int i = 0; i < crs->import_table1_num; ++i) {
        ImportTableEntry* entry = crs->GetImportTable1Entry(i);
        Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(entry->symbol_offset));
        if (first_patch->unk2)
            if (FindExportByName(unload, reinterpret_cast<char*>(Memory::GetPointer(entry->name_offset))))
                UnloadImportTablePatches(crs, first_patch, base_offset);
    }
}

static void UnloadUnk2Patches(CROHeader* cro, CROHeader* unload, u32 base_offset) {
    char* unload_name = reinterpret_cast<char*>(Memory::GetPointer(unload->name_offset));
    for (int i = 0; i < cro->unk2_num; ++i) {
        Unk2Patch* entry = cro->GetUnk2PatchEntry(i);
        // Find the patch that corresponds to the CRO that is being unloaded
        if (strcmp(reinterpret_cast<char*>(Memory::GetPointer(entry->string_offset)), unload_name) == 0) {

            // Apply the table 1 patches
            for (int j = 0; j < entry->table1_num; ++j) {
                Unk2TableEntry* table1_entry = entry->GetTable1Entry(j);
                Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(table1_entry->patches_offset));
                UnloadImportTablePatches(cro, first_patch, base_offset);
            }

            // Apply the table 2 patches
            for (int j = 0; j < entry->table1_num; ++j) {
                Unk2TableEntry* table2_entry = entry->GetTable2Entry(j);
                Patch* first_patch = reinterpret_cast<Patch*>(Memory::GetPointer(table2_entry->patches_offset));
                UnloadImportTablePatches(cro, first_patch, base_offset);
            }
            break;
        }
    }
}

static void ApplyCRSUnloadPatches(CROHeader* crs, CROHeader& unload) {
    u32 base_offset = CalculateBaseOffset(crs);

    ApplyCRSImportTable1UnloadPatches(crs, unload, base_offset);
}

static void UnrebaseImportTable3(CROHeader* cro, u32 address) {
    for (int i = 0; i < cro->import_table3_num; ++i) {
        ImportTableEntry* entry = cro->GetImportTable3Entry(i);
        if (entry->symbol_offset)
            entry->symbol_offset -= address;
    }
}

static void UnrebaseImportTable2(CROHeader* cro, u32 address) {
    for (int i = 0; i < cro->import_table2_num; ++i) {
        ImportTableEntry* entry = cro->GetImportTable2Entry(i);
        if (entry->symbol_offset)
            entry->symbol_offset -= address;
    }
}

static void UnrebaseImportTable1(CROHeader* cro, u32 address) {
    for (int i = 0; i < cro->import_table1_num; ++i) {
        ImportTableEntry* entry = cro->GetImportTable1Entry(i);
        if (entry->name_offset)
            entry->name_offset -= address;
        if (entry->symbol_offset)
            entry->symbol_offset -= address;
    }
}

static void UnrebaseUnk2Patches(CROHeader* cro, u32 address) {
    for (int i = 0; i < cro->unk2_num; ++i) {
        Unk2Patch* entry = cro->GetUnk2PatchEntry(i);
        if (entry->string_offset)
            entry->string_offset -= address;
        if (entry->table1_offset)
            entry->table1_offset -= address;
        if (entry->table2_offset)
            entry->table2_offset -= address;
    }
}

static void UnrebaseExportsTable(CROHeader* cro, u32 address) {
    for (int i = 0; i < cro->export_table_num; ++i) {
        ExportTableEntry* entry = cro->GetExportTableEntry(i);
        if (entry->name_offset)
            entry->name_offset -= address;
    }
}

static void UnrebaseSegments(CROHeader* cro, u32 address) {
    for (int i = 0; i < cro->segment_table_num; ++i) {
        SegmentTableEntry entry = cro->GetSegmentTableEntry(i);
        if (entry.segment_id == 3)
            entry.segment_offset = 0;
        else if (entry.segment_id)
            entry.segment_offset -= address;
        cro->SetSegmentTableEntry(i, entry);
    }
}

static void UnrebaseCRO(CROHeader* cro, u32 address) {
    UnrebaseImportTable3(cro, address);
    UnrebaseImportTable2(cro, address);
    UnrebaseImportTable1(cro, address);
    UnrebaseUnk2Patches(cro, address);
    UnrebaseExportsTable(cro, address);
    UnrebaseSegments(cro, address);

    if (cro->name_offset)
        cro->name_offset -= address;

    if (cro->code_offset)
        cro->code_offset -= address;

    if (cro->unk_offset)
        cro->unk_offset -= address;

    if (cro->module_name_offset)
        cro->module_name_offset -= address;

    if (cro->segment_table_offset)
        cro->segment_table_offset -= address;

    if (cro->export_table_offset)
        cro->export_table_offset -= address;

    if (cro->unk1_offset)
        cro->unk1_offset -= address;

    if (cro->export_strings_offset)
        cro->export_strings_offset -= address;

    if (cro->export_tree_offset)
        cro->export_tree_offset -= address;

    if (cro->unk2_offset)
        cro->unk2_offset -= address;

    if (cro->import_patches_offset)
        cro->import_patches_offset -= address;

    if (cro->import_table1_offset)
        cro->import_table1_offset -= address;

    if (cro->import_table2_offset)
        cro->import_table2_offset -= address;

    if (cro->import_table3_offset)
        cro->import_table3_offset -= address;

    if (cro->import_strings_offset)
        cro->import_strings_offset -= address;

    if (cro->unk3_offset)
        cro->unk3_offset -= address;

    if (cro->relocation_patches_offset)
        cro->relocation_patches_offset -= address;

    if (cro->unk4_offset)
        cro->unk4_offset -= address;
}

static void UnloadExports(u32 address) {
    for (auto itr = loaded_exports.begin(); itr != loaded_exports.end();) {
        if (itr->second.cro_base == address)
            itr = loaded_exports.erase(itr);
        else
            ++itr;
    }
}

static ResultCode UnloadCRO(u32 address) {
    // If there's only one loaded CRO, it must be the CRS, which can not be unloaded like this
    if (loaded_cros.size() == 1) {
        return ResultCode(0xD9012C1E);
    }

    CROHeader* crs = reinterpret_cast<CROHeader*>(Memory::GetPointer(loaded_cros.front()));
    CROHeader* unload = reinterpret_cast<CROHeader*>(Memory::GetPointer(address));
    u32 size = unload->file_size;

    UnlinkCRO(crs, unload, address);

    u32 base_offset = CalculateBaseOffset(unload);

    UnloadImportTable1Patches(unload, base_offset);
    UnloadImportTable2Patches(unload, base_offset);
    UnloadImportTable3Patches(unload, base_offset);

    for (u32 base : loaded_cros) {
        if (base == address)
            continue;
        CROHeader* cro = reinterpret_cast<CROHeader*>(Memory::GetPointer(base));
        ApplyCRSUnloadPatches(cro, *unload);
        base_offset = CalculateBaseOffset(cro);
        UnloadUnk2Patches(cro, unload, base_offset);
    }

    UnrebaseCRO(unload, address);
    unload->unk_address = 0;

    loaded_cros.erase(std::remove(loaded_cros.begin(), loaded_cros.end(), address), loaded_cros.end());

    UnloadExports(address);

    Kernel::g_current_process->vm_manager.UnmapRange(address, size);

    // TODO(Subv): Unload symbols and unmap memory
    return RESULT_SUCCESS;
}

static void UnloadCRO(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 address = cmd_buff[1];
    ResultCode res = UnloadCRO(address);
    cmd_buff[1] = res.raw;
    // Clear the instruction cache
    Core::g_app_core->ClearInstructionCache();
    LOG_WARNING(Service_LDR, "Unloading CRO address=%08X res=%08X", address, res);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C2, Initialize,            "Initialize"},
    {0x00020082, LoadCRR,               "LoadCRR"},
    {0x00030042, nullptr,               "UnloadCCR"},
    {0x000402C2, LoadExeCRO,            "LoadExeCRO"},
    {0x000500C2, UnloadCRO,             "UnloadCRO"},
    {0x00060042, nullptr,               "CRO_Load?"},
    {0x00070042, nullptr,               "LoadCROSymbols"},
    {0x00080042, nullptr,               "Shutdown"},
    {0x000902C2, nullptr,               "LoadExeCRO_New?"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
