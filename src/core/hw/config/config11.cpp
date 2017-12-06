// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hw/config/config11.h"
#include "core/memory.h"

namespace HW {
namespace Config {

std::shared_ptr<Config11MMIO> Config11;

bool Config11MMIO::IsValidAddress(VAddr addr) {
    return VirtualToPhysicalAddress(addr) != boost::none;
}

bool Config11MMIO::ReadBlock(VAddr src_addr, void* dest_buffer, size_t size) {
    ASSERT(false);
    return false;
}

bool Config11MMIO::WriteBlock(VAddr dest_addr, const void* src_buffer, size_t size) {
    ASSERT(false);
    return false;
}

u8 Config11MMIO::Read8(VAddr addr) {
    u8 val;
    std::memcpy(&val, &regs.reg_array[OffsetFromAddr(addr)], sizeof(u8));
    return val;
}

u16 Config11MMIO::Read16(VAddr addr) {
    u16 val;
    std::memcpy(&val, &regs.reg_array[OffsetFromAddr(addr)], sizeof(u16));
    return val;
}

u32 Config11MMIO::Read32(VAddr addr) {
    u32 val;
    std::memcpy(&val, &regs.reg_array[OffsetFromAddr(addr)], sizeof(u32));
    return val;
}

u64 Config11MMIO::Read64(VAddr addr) {
    u64 val;
    std::memcpy(&val, &regs.reg_array[OffsetFromAddr(addr)], sizeof(u64));
    return val;
}

void Config11MMIO::Write8(VAddr addr, u8 data) {
    std::memcpy(&regs.reg_array[OffsetFromAddr(addr)], &data, sizeof(u8));
}

void Config11MMIO::Write16(VAddr addr, u16 data) {
    std::memcpy(&regs.reg_array[OffsetFromAddr(addr)], &data, sizeof(u16));
}

void Config11MMIO::Write32(VAddr addr, u32 data) {
    std::memcpy(&regs.reg_array[OffsetFromAddr(addr)], &data, sizeof(u32));
}

void Config11MMIO::Write64(VAddr addr, u64 data) {
    std::memcpy(&regs.reg_array[OffsetFromAddr(addr)], &data, sizeof(u64));
}

boost::optional<PAddr> Config11MMIO::VirtualToPhysicalAddress(VAddr address) {
    auto paddr = Memory::TryVirtualToPhysicalAddress(address);
    if (paddr == boost::none)
        return boost::none;

    if (*paddr < Config11Begin || *paddr >= Config11Begin + Config11Size)
        return boost::none;

    return paddr;
}

std::size_t Config11MMIO::OffsetFromAddr(VAddr address) {
    PAddr paddr = *VirtualToPhysicalAddress(address);

    return paddr & (Config11Size - 1);
}

} // namespace Config
} // namespace HW
