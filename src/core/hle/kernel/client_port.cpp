// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

ClientPort::ClientPort() {}
ClientPort::~ClientPort() {}

ResultVal<SharedPtr<ClientPort>> ClientPort::Create(SharedPtr<ServerPort> server_port, u32 max_sessions, std::string name) {
    SharedPtr<ClientPort> client_port(new ClientPort);

    client_port->server_port = server_port;
    client_port->max_sessions = max_sessions;
    client_port->active_sessions = 0;
    client_port->name = std::move(name);

    return MakeResult<SharedPtr<ClientPort>>(std::move(client_port));
}

void ClientPort::AddWaitingSession(SharedPtr<ServerSession> server_session) {
    server_port->pending_sessions.push_back(server_session);
    // Wake the threads waiting on the ServerPort
    server_port->WakeupAllWaitingThreads();
}

} // namespace
