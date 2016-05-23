// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

class ServerPort;
class ServerSession;

class ClientPort : public Object {
public:
    /**
     * Creates a client port.
     * @param server_port ServerPort associated with this client port.
     * @param max_sessions Maximum number of sessions to this port
     * @param name Optional name of client port
     * @return The created client port
     */
    static ResultVal<SharedPtr<ClientPort>> Create(SharedPtr<ServerPort> server_port, u32 max_sessions, std::string name = "Unknown");

    /**
     * Adds the specified server session to the queue of pending sessions of the associated ServerPort
     * @param server_session Server session to add to the queue
     */
    virtual void AddWaitingSession(SharedPtr<ServerSession> server_session);

    /**
     * Handle a sync request from the emulated application.
     * Only HLE services should override this function.
     * @returns ResultCode from the operation.
     */
    virtual ResultCode HandleSyncRequest() { return RESULT_SUCCESS; }

    std::string GetTypeName() const override { return "ClientPort"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::ClientPort;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    SharedPtr<ServerPort> server_port;          ///< ServerPort associated with this client port.
    u32 max_sessions;                           ///< Maximum number of simultaneous sessions the port can have
    u32 active_sessions;                        ///< Number of currently open sessions to this port
    std::string name;                           ///< Name of client port (optional)

    ClientPort();
    ~ClientPort() override;
};

} // namespace
