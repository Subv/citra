// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/soc_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SOC_U

namespace SOC_U {

static void SOCU_socket(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 domain = cmd_buffer[1]; // Address family
    u32 type = cmd_buffer[2];
    u32 protocol = cmd_buffer[3];

    if (protocol != 0) {
        cmd_buffer[1] = UnimplementedFunction(ErrorModule::SOC).raw; // TODO(Subv): Correct error code
        return;
    }

    if (domain != AF_INET) {
        cmd_buffer[1] = UnimplementedFunction(ErrorModule::SOC).raw; // TODO(Subv): Correct error code
        return;
    }

    if (type != SOCK_DGRAM && type != SOCK_STREAM) {
        cmd_buffer[1] = UnimplementedFunction(ErrorModule::SOC).raw; // TODO(Subv): Correct error code
        return;
    }

    SOCKET fd = socket(domain, type, protocol);
    cmd_buffer[2] = fd;
    cmd_buffer[1] = 0;
}

static void SOCU_bind(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    SOCKET sock = (SOCKET)cmd_buffer[1];
    u32 len = cmd_buffer[2];
    sockaddr* sock_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[6]));
    sock_addr->sa_family >>= 8; // Discard the low 8 bits (length)

    cmd_buffer[2] = bind(sock, sock_addr, sizeof(sockaddr_in));
    cmd_buffer[1] = 0;
}

static void SOCU_fcntl(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    SOCKET sock = (SOCKET)cmd_buffer[1];
    u32 cmd = cmd_buffer[2];
    u32 arg = cmd_buffer[3];

    u32 ret;
#ifdef _WIN32
    // Windows uses different enum values with different meanings,
    // cmd 4 is F_SETFL and arg 4 is O_NONBLOCK
    if (cmd == 4 && arg == 4) {
        cmd = FIONBIO;
        arg = 1;
    }
    ret = ioctlsocket(sock, cmd, reinterpret_cast<u_long*>(&arg));
#else
    ret = fcntl(sock, cmd, arg);
#endif // _WIN32

    cmd_buffer[2] = ret;
    cmd_buffer[1] = 0;
}

static void SOCU_listen(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    SOCKET sock = (SOCKET)cmd_buffer[1];
    u32 backlog = cmd_buffer[2];

    cmd_buffer[2] = listen(sock, backlog);
    cmd_buffer[1] = 0;
}

static void SOCU_accept(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    SOCKET sock = (SOCKET)cmd_buffer[1];
    int max_addr_len = static_cast<int>(cmd_buffer[2]);
    sockaddr addr;

    u32 ret = accept(sock, &addr, &max_addr_len);
    
    // TODO(Subv): Write addr to the pointer located at cmd_buffer[0x104 >> 2]

    cmd_buffer[2] = ret;
    cmd_buffer[1] = 0;
}

static void SOCU_gethostid(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();

    // TODO(Subv): Implement
    cmd_buffer[2] = 0;
    cmd_buffer[1] = 0;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, nullptr,                       "InitializeSockets"},
    {0x000200C2, SOCU_socket,                   "socket"},
    {0x00030082, SOCU_listen,                   "listen"},
    {0x00040082, SOCU_accept,                   "accept"},
    {0x00050084, SOCU_bind,                     "bind"},
    {0x00060084, nullptr,                       "connect"},
    {0x00070104, nullptr,                       "recvfrom_other"},
    {0x00080102, nullptr,                       "recvfrom"},
    {0x00090106, nullptr,                       "sendto_other"},
    {0x000A0106, nullptr,                       "sendto"},
    {0x000B0042, nullptr,                       "close"},
    {0x000C0082, nullptr,                       "shutdown"},
    {0x000D0082, nullptr,                       "gethostbyname"},
    {0x000E00C2, nullptr,                       "gethostbyaddr"},
    {0x000F0106, nullptr,                       "unknown_resolve_ip"},
    {0x00110102, nullptr,                       "getsockopt"},
    {0x00120104, nullptr,                       "setsockopt"},
    {0x001300C2, SOCU_fcntl,                    "fcntl"},
    {0x00140084, nullptr,                       "poll"},
    {0x00150042, nullptr,                       "sockatmark"},
    {0x00160000, SOCU_gethostid,                "gethostid"},
    {0x00170082, nullptr,                       "getsockname"},
    {0x00180082, nullptr,                       "getpeername"},
    {0x00190000, nullptr,                       "ShutdownSockets"},
    {0x001A00C0, nullptr,                       "GetNetworkOpt"},
    {0x001B0040, nullptr,                       "ICMPSocket"},
    {0x001C0104, nullptr,                       "ICMPPing"},
    {0x001D0040, nullptr,                       "ICMPCancel"},
    {0x001E0040, nullptr,                       "ICMPClose"},
    {0x001F0040, nullptr,                       "GetResolverInfo"},
    {0x00210002, nullptr,                       "CloseSockets"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif // _WIN32
}

Interface::~Interface() {
#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace
