// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/platform.h"

#if EMU_PLATFORM == PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/soc_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SOC_U

namespace SOC_U {

static void socket(Service::Interface* self) {
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

    cmd_buffer[2] = ::socket(domain, type, protocol);
    cmd_buffer[1] = 0;
}

static void bind(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 len = cmd_buffer[2];
    sockaddr* sock_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[6]));
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    sock_addr->sa_family >>= 8; 
#endif
    cmd_buffer[2] = ::bind(socket_handle, sock_addr, sizeof(sockaddr_in));
    cmd_buffer[1] = 0;
}

static void fcntl(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 cmd = cmd_buffer[2];
    u32 arg = cmd_buffer[3];

    u32 ret;
#if EMU_PLATFORM == PLATFORM_WINDOWS
    // Windows uses different enum values with different meanings,
    // cmd 4 is F_SETFL and arg 4 is O_NONBLOCK
    if (cmd == 4 && arg == 4) {
        cmd = FIONBIO;
        arg = 1;
    }
    ret = ioctlsocket(socket_handle, cmd, reinterpret_cast<u_long*>(&arg));
#else
    ret = ::fcntl(socket_handle, cmd, arg);
#endif // _WIN32

    cmd_buffer[2] = ret;
    cmd_buffer[1] = 0;
}

static void listen(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 backlog = cmd_buffer[2];

    cmd_buffer[2] = ::listen(socket_handle, backlog);
    cmd_buffer[1] = 0;
}

static void accept(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    socklen_t max_addr_len = static_cast<socklen_t>(cmd_buffer[2]);
    sockaddr addr;

    u32 ret = ::accept(socket_handle, &addr, &max_addr_len);
    
    // TODO(Subv): Write addr to the pointer located at cmd_buffer[0x104 >> 2]

    cmd_buffer[2] = ret;
    cmd_buffer[1] = 0;
}

static void gethostid(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();

    char name[128];
    gethostname(name, sizeof(name));
    hostent* host = gethostbyname(name);
    in_addr* addr = reinterpret_cast<in_addr*>(host->h_addr);

    cmd_buffer[2] = addr->s_addr;
    cmd_buffer[1] = 0;
}

static void close(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];

#if EMU_PLATFORM == PLATFORM_WINDOWS
    cmd_buffer[2] = ::closesocket(socket_handle);
#else
    cmd_buffer[2] = ::close(socket_handle);
#endif
    cmd_buffer[1] = 0;
}

static void sendto(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 len = cmd_buffer[2];
    u32 flags = cmd_buffer[3];
    u32 addr_len = cmd_buffer[4];

    u8* input_buff = Memory::GetPointer(cmd_buffer[8]);
    sockaddr* dest_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[10]));

    cmd_buffer[2] = ::sendto(socket_handle, (const char*)input_buff, len, flags, dest_addr, addr_len);
    cmd_buffer[1] = 0;
}

static void recvfrom(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 len = cmd_buffer[2];
    u32 flags = cmd_buffer[3];
    socklen_t addr_len = static_cast<socklen_t>(cmd_buffer[4]);

    u8* output_buff = Memory::GetPointer(cmd_buffer[0x104 >> 2]);
    sockaddr* src_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[0x1A0 >> 2]));

    cmd_buffer[2] = ::recvfrom(socket_handle, (char*)output_buff, len, flags, src_addr, &addr_len);
    cmd_buffer[1] = 0;
}

static void InitializeSockets(Service::Interface* self) {
    // TODO(Subv): Implement
#if EMU_PLATFORM == PLATFORM_WINDOWS
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif // _WIN32
}

static void ShutdownSockets(Service::Interface* self) {
    // TODO(Subv): Implement
#if EMU_PLATFORM == PLATFORM_WINDOWS
    WSACleanup();
#endif
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, InitializeSockets,             "InitializeSockets"},
    {0x000200C2, socket,                        "socket"},
    {0x00030082, listen,                        "listen"},
    {0x00040082, accept,                        "accept"},
    {0x00050084, bind,                          "bind"},
    {0x00060084, nullptr,                       "connect"},
    {0x00070104, nullptr,                       "recvfrom_other"},
    {0x00080102, recvfrom,                      "recvfrom"},
    {0x00090106, nullptr,                       "sendto_other"},
    {0x000A0106, sendto,                        "sendto"},
    {0x000B0042, close,                         "close"},
    {0x000C0082, nullptr,                       "shutdown"},
    {0x000D0082, nullptr,                       "gethostbyname"},
    {0x000E00C2, nullptr,                       "gethostbyaddr"},
    {0x000F0106, nullptr,                       "unknown_resolve_ip"},
    {0x00110102, nullptr,                       "getsockopt"},
    {0x00120104, nullptr,                       "setsockopt"},
    {0x001300C2, fcntl,                         "fcntl"},
    {0x00140084, nullptr,                       "poll"},
    {0x00150042, nullptr,                       "sockatmark"},
    {0x00160000, gethostid,                     "gethostid"},
    {0x00170082, nullptr,                       "getsockname"},
    {0x00180082, nullptr,                       "getpeername"},
    {0x00190000, ShutdownSockets,               "ShutdownSockets"},
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
}

Interface::~Interface() {
}

} // namespace
