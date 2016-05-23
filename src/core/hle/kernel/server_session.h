// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/assert.h"
#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace IPC {

constexpr u32 MakeHeader(u16 command_id, unsigned int regular_params, unsigned int translate_params) {
    return ((u32)command_id << 16) | (((u32)regular_params & 0x3F) << 6) | (((u32)translate_params & 0x3F) << 0);
}

constexpr u32 MoveHandleDesc(unsigned int num_handles = 1) {
    return 0x0 | ((num_handles - 1) << 26);
}

constexpr u32 CopyHandleDesc(unsigned int num_handles = 1) {
    return 0x10 | ((num_handles - 1) << 26);
}

constexpr u32 CallingPidDesc() {
    return 0x20;
}

constexpr u32 StaticBufferDesc(u32 size, unsigned int buffer_id) {
    return 0x2 | (size << 14) | ((buffer_id & 0xF) << 10);
}

enum MappedBufferPermissions {
    R = 2,
    W = 4,
    RW = R | W,
};

constexpr u32 MappedBufferDesc(u32 size, MappedBufferPermissions perms) {
    return 0x8 | (size << 4) | (u32)perms;
}

}

namespace Kernel {

static const int kCommandHeaderOffset = 0x80; ///< Offset into command buffer of header

/**
 * Returns a pointer to the command buffer in the current thread's TLS
 * TODO(Subv): This is not entirely correct, the command buffer should be copied from
 * the thread's TLS to an intermediate buffer in kernel memory, and then copied again to
 * the service handler process' memory.
 * @param offset Optional offset into command buffer
 * @return Pointer to command buffer
 */
inline static u32* GetCommandBuffer(const int offset = 0) {
    return (u32*)Memory::GetPointer(GetCurrentThread()->GetTLSAddress() + kCommandHeaderOffset + offset);
}

class ClientSession;
class ClientPort;

/**
 * Kernel object representing the server endpoint of an IPC session. Sessions are the basic CTR-OS
 * primitive for communication between different processes, and are used to implement service calls
 * to the various system services.
 *
 * To make a service call, the client must write the command header and parameters to the buffer
 * located at offset 0x80 of the TLS (Thread-Local Storage) area, then execute a SendSyncRequest
 * SVC call with its ClientSession handle. The kernel will read the command header, using it to marshall
 * the parameters to the process at the server endpoint of the session. After the server replies to
 * the request, the response is marshalled back to the caller's TLS buffer and control is
 * transferred back to it.
 */
class ServerSession : public WaitObject {
public:
    ServerSession();
    ~ServerSession() override;

    /**
     * Creates a server session.
     * @param name Optional name of the server session
     * @return The created server session
     */
    static ResultVal<SharedPtr<ServerSession>> Create(std::string name = "Unknown");

    std::string GetTypeName() const override { return "ServerSession"; }

    static const HandleType HANDLE_TYPE = HandleType::ServerSession;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    /**
     * Creates a pair of ServerSession and an associated ClientSession.
     * @param client_port ClientPort to which the sessions are connected
     * @param name Optional name of the ports
     * @return The created session tuple
     */
    static std::tuple<SharedPtr<ServerSession>, SharedPtr<ClientSession>> CreateSessionPair(SharedPtr<ClientPort> client_port, std::string name = "Unknown");

    /**
     * Creates a portless ClientSession and associates it with this ServerSession.
     * @returns ClientSession The newly created ClientSession.
     */
    SharedPtr<ClientSession> CreateClientSession();

    /**
     * Handle a sync request from the emulated application.
     * Only HLE services should override this function.
     * @returns ResultCode from the operation.
     */
    virtual ResultCode HandleSyncRequest();

    bool ShouldWait() override;

    void Acquire() override;

    std::string name; ///< The name of this session (optional)
    bool signaled;    ///< Whether there's new data available to this ServerSession
};

}
