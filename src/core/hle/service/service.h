// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include <boost/container/flat_map.hpp>

#include "common/common_types.h"

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

static const int kMaxPortSize = 8; ///< Maximum size of a port name (8 characters)
static const u32 DefaultMaxSessions = 10; ///< Arbitrary default number of maximum connections to an HLE port

/// Interface to a CTROS service
class Interface : public Kernel::ClientPort {
    // TODO(yuriks): An "Interface" being a Kernel::Object is mostly non-sense. Interface should be
    // just something that encapsulates a session and acts as a helper to implement service
    // processes.
public:
    std::string GetName() const override { return GetPortName(); }

    /**
     * Gets the maximum allowed number of sessions that can be connected to this port at the same time.
     * It should be overwritten by each service implementation for more fine-grained control.
     * @returns The maximum number of connections allowed.
     */
    virtual u32 GetMaxSessions() { return DefaultMaxSessions; }

    void AddWaitingSession(Kernel::SharedPtr<Kernel::ServerSession> server_session) override { }

    typedef void (*Function)(Interface*);

    struct FunctionInfo {
        u32         id;
        Function    func;
        const char* name;
    };

    /**
     * Gets the string name used by CTROS for a service
     * @return Port name of service
     */
    virtual std::string GetPortName() const {
        return "[UNKNOWN SERVICE PORT]";
    }

    ResultCode HandleSyncRequest() override;

protected:

    /**
     * Registers the functions in the service
     */
    template <size_t N>
    inline void Register(const FunctionInfo (&functions)[N]) {
        Register(functions, N);
    }

    void Register(const FunctionInfo* functions, size_t n);

private:
    boost::container::flat_map<u32, FunctionInfo> m_functions;

};

/// Initialize ServiceManager
void Init();

/// Shutdown ServiceManager
void Shutdown();

/// Map of named ports managed by the kernel, which can be retrieved using the ConnectToPort SVC.
extern std::unordered_map<std::string, Kernel::SharedPtr<Kernel::ClientPort>> g_kernel_named_ports;
/// Map of services registered with the "srv:" service, retrieved using GetServiceHandle.
extern std::unordered_map<std::string, Kernel::SharedPtr<Kernel::ClientPort>> g_srv_services;

/// Adds a service to the services table
void AddService(Interface* interface_);

} // namespace
