// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <boost/optional.hpp>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/mmio.h"

namespace HW {
namespace Config {

/**
 * Represents the device that controls the CONFIG11 IO registers.
 */
class Config11MMIO final : public Memory::MMIORegion {
public:
    ~Config11MMIO() override = default;

    bool IsValidAddress(VAddr addr) override;

    u8 Read8(VAddr addr) override;
    u16 Read16(VAddr addr) override;
    u32 Read32(VAddr addr) override;
    u64 Read64(VAddr addr) override;

    bool ReadBlock(VAddr src_addr, void* dest_buffer, size_t size) override;

    void Write8(VAddr addr, u8 data) override;
    void Write16(VAddr addr, u16 data) override;
    void Write32(VAddr addr, u32 data) override;
    void Write64(VAddr addr, u64 data) override;

    bool WriteBlock(VAddr dest_addr, const void* src_buffer, size_t size) override;

private:
    static constexpr PAddr Config11Begin = 0x10140000;
    static constexpr PAddr Config11Size = 0x2000;

    /// Converts a virtual address in the Config11 address space into a physical address.
    boost::optional<PAddr> VirtualToPhysicalAddress(VAddr address);
    /// Retrieves the offset into the Config11 pages by masking off the top bits of the address.
    std::size_t OffsetFromAddr(VAddr address);

    struct Regs {
        union {
            struct {
                INSERT_PADDING_BYTES(0x180);
                u8 CFG11_WIFICNT;
                INSERT_PADDING_BYTES(0x1E7F);
            };

            std::array<u8, Config11Size> reg_array;
        };
    };

    static_assert(sizeof(Regs) == Config11Size, "Incorrect region size.");

    Regs regs{};
};

extern std::shared_ptr<Config11MMIO> Config11;

}; // namespace Config
}; // namespace HW
