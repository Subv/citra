// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/platform.h"

#if EMU_PLATFORM == PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#endif

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/soc_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SOC_U

namespace SOC_U {

/// Structure to represent the 3ds' pollfd structure, which is different than most implementations
struct hw_pollfd {
    u32 fd; ///< Socket handle
    u32 events; ///< Events to poll for (input)
    u32 revents; ///< Events received (output)
};

#if EMU_PLATFORM == PLATFORM_WINDOWS
/// Holds info about whether a specific socket is blocking or not
/// This only exists on Windows because it has no way of querying if a socket is blocking or not
std::unordered_map<u32, bool> socket_blocking;
#endif

static void Socket(Service::Interface* self) {
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

    u32 socket_handle = ::socket(domain, type, protocol);

#if EMU_PLATFORM == PLATFORM_WINDOWS
    socket_blocking[socket_handle] = true;
#endif

    cmd_buffer[2] = socket_handle;
    cmd_buffer[1] = 0;
}

static void Bind(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 len = cmd_buffer[2];
    sockaddr* sock_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[6]));
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    if (sock_addr != nullptr)
        sock_addr->sa_family >>= 8;
#endif
    cmd_buffer[2] = ::bind(socket_handle, sock_addr, sizeof(sockaddr_in));
    cmd_buffer[1] = 0;
}

static void Fcntl(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 cmd = cmd_buffer[2];
    u32 arg = cmd_buffer[3];

    u32 ret;
#if EMU_PLATFORM == PLATFORM_WINDOWS
    // Windows uses different enum values with different meanings,
    // cmd 4 is F_SETFL and arg 4 is O_NONBLOCK
    if (cmd == 4 && arg & 4) {
        cmd = FIONBIO;
        arg = 1;
        ret = ioctlsocket(socket_handle, cmd, reinterpret_cast<u_long*>(&arg));
        socket_blocking[socket_handle] = false;
    } else if (cmd == 3) {
        // 3 is F_GETFL, this is used to check if a socket is nonblocking
        // Return O_NONBLOCK (4) if the socket is nonblocking
        ret = socket_blocking[socket_handle] ? 0 : 4;
    }
#else
    ret = ::fcntl(socket_handle, cmd, arg);
#endif // _WIN32

    cmd_buffer[2] = ret;
    cmd_buffer[1] = 0;
}

static void Listen(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 backlog = cmd_buffer[2];

    cmd_buffer[2] = ::listen(socket_handle, backlog);
    cmd_buffer[1] = 0;
}

static void Accept(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    socklen_t max_addr_len = static_cast<socklen_t>(cmd_buffer[2]);
    sockaddr addr;

    u32 ret = ::accept(socket_handle, &addr, &max_addr_len);
    
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    addr.sa_family = (addr.sa_family << 8) | max_addr_len;
#endif

#if EMU_PLATFORM == PLATFORM_WINDOWS
    if (ret != SOCKET_ERROR)
        socket_blocking[ret] = true;
#endif

    Memory::WriteBlock(cmd_buffer[0x104 >> 2], reinterpret_cast<const u8*>(&addr), max_addr_len);
    cmd_buffer[2] = ret;
    cmd_buffer[1] = 0;
}

static void GetHostId(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();

    char name[128];
    gethostname(name, sizeof(name));
    hostent* host = gethostbyname(name);
    in_addr* addr = reinterpret_cast<in_addr*>(host->h_addr);

    cmd_buffer[2] = addr->s_addr;
    cmd_buffer[1] = 0;
}

static void Close(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];

#if EMU_PLATFORM == PLATFORM_WINDOWS
    cmd_buffer[2] = ::closesocket(socket_handle);
    socket_blocking.erase(socket_handle);
#else
    cmd_buffer[2] = ::close(socket_handle);
#endif
    cmd_buffer[1] = 0;
}

static void SendTo(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 len = cmd_buffer[2];
    u32 flags = cmd_buffer[3];
    u32 addr_len = cmd_buffer[4];

    u8* input_buff = Memory::GetPointer(cmd_buffer[8]);
    sockaddr* dest_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[10]));

    cmd_buffer[2] = ::sendto(socket_handle, (const char*)input_buff, len, flags, dest_addr, addr_len);
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    dest_addr->sa_family = (dest_addr->sa_family << 8) | addr_len;
#endif
    cmd_buffer[1] = 0;
}

static void RecvFrom(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    u32 len = cmd_buffer[2];
    u32 flags = cmd_buffer[3];
    socklen_t addr_len = static_cast<socklen_t>(cmd_buffer[4]);

    u8* output_buff = Memory::GetPointer(cmd_buffer[0x104 >> 2]);
    sockaddr* src_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[0x1A0 >> 2]));

    cmd_buffer[2] = ::recvfrom(socket_handle, (char*)output_buff, len, flags, src_addr, &addr_len);
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    if (src_addr != nullptr)
        src_addr->sa_family = (src_addr->sa_family << 8) | addr_len;
#endif
    cmd_buffer[1] = 0;
}

/// Translates the resulting events of a Poll operation from platform-specific to 3ds specific
static u32 translate_poll_event_3ds(u32 input_event) {
    u32 ret = 0;
    if (input_event & POLLIN)
        ret |= 1;
    if (input_event & POLLPRI)
        ret |= 2;
    if (input_event & POLLHUP)
        ret |= 4;
    if (input_event & POLLERR)
        ret |= 8;
    if (input_event & POLLOUT)
        ret |= 0x10;
    if (input_event & POLLNVAL)
        ret |= 0x20;
    return ret;
}

/// Translates the resulting events of a Poll operation from 3ds specific to platform specific
static u32 translate_poll_event_platform(u32 input_event) {
    u32 ret = 0;
    if (input_event & 1)
        ret |= POLLIN;
    if (input_event & 2)
        ret |= POLLPRI;
    if (input_event & 4)
        ret |= POLLHUP;
    if (input_event & 8)
        ret |= POLLERR;
    if (input_event & 0x10)
        ret |= POLLOUT;
    if (input_event & 0x20)
        ret |= POLLNVAL;
    return ret;
}

static void Poll(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 nfds = cmd_buffer[1];
    int timeout = cmd_buffer[2];
    hw_pollfd* input_fds = reinterpret_cast<hw_pollfd*>(Memory::GetPointer(cmd_buffer[6]));
    hw_pollfd* output_fds = reinterpret_cast<hw_pollfd*>(Memory::GetPointer(cmd_buffer[0x104 >> 2]));

    // The 3ds_pollfd and the pollfd structures may be different (Windows/Linux have different sizes)
    // so we have to copy the data
    pollfd* platform_pollfd = new pollfd[nfds];
    for (int current_fds = 0; current_fds < nfds; ++current_fds) {
        platform_pollfd[current_fds].fd = input_fds[current_fds].fd;
        platform_pollfd[current_fds].events = translate_poll_event_platform(input_fds[current_fds].events);
        platform_pollfd[current_fds].revents = translate_poll_event_platform(input_fds[current_fds].revents);
    }
    
    int ret;
#if EMU_PLATFORM == PLATFORM_WINDOWS
    ret = WSAPoll(platform_pollfd, nfds, timeout);
#else
    ret = ::poll(platform_pollfd, nfds, timeout);
#endif

    // Now update the output pollfd structure
    for (int current_fds = 0; current_fds < nfds; ++current_fds) {
        output_fds[current_fds].fd = platform_pollfd[current_fds].fd;
        output_fds[current_fds].events = translate_poll_event_3ds(platform_pollfd[current_fds].events);
        output_fds[current_fds].revents = translate_poll_event_3ds(platform_pollfd[current_fds].revents);
    }

    delete[] platform_pollfd;

    cmd_buffer[1] = 0;
    cmd_buffer[2] = ret;
}

static void GetSockName(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    socklen_t len = cmd_buffer[2];

    sockaddr* dest_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[0x104 >> 2]));

    cmd_buffer[2] = ::getsockname(socket_handle, dest_addr, &len);
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    if (dest_addr != nullptr)
        dest_addr->sa_family = (dest_addr->sa_family << 8) | len;
#endif
    cmd_buffer[1] = 0;
}

static void Shutdown(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    int how = cmd_buffer[2];

    cmd_buffer[2] = ::shutdown(socket_handle, how);
    cmd_buffer[1] = 0;
}

static void GetPeerName(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    socklen_t len = cmd_buffer[2];

    sockaddr* dest_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[0x104 >> 2]));

    cmd_buffer[2] = ::getpeername(socket_handle, dest_addr, &len);
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    if (dest_addr != nullptr)
        dest_addr->sa_family = (dest_addr->sa_family << 8) | len;
#endif
    cmd_buffer[1] = 0;
}

static void Connect(Service::Interface* self) {
    u32* cmd_buffer = Service::GetCommandBuffer();
    u32 socket_handle = cmd_buffer[1];
    socklen_t len = cmd_buffer[2];

    sockaddr* input_addr = reinterpret_cast<sockaddr*>(Memory::GetPointer(cmd_buffer[6]));

    cmd_buffer[2] = ::connect(socket_handle, input_addr, len);
#if EMU_PLATFORM != PLATFORM_MACOSX
    // OS X uses the first byte for the struct length, Windows doesn't
    if (input_addr != nullptr)
        input_addr->sa_family = (input_addr->sa_family << 8) | len;
#endif
    cmd_buffer[1] = 0;
}

static void InitializeSockets(Service::Interface* self) {
    // TODO(Subv): Implement
#if EMU_PLATFORM == PLATFORM_WINDOWS
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif // _WIN32

    u32* cmd_buffer = Service::GetCommandBuffer();
    cmd_buffer[1] = 0;
}

static void ShutdownSockets(Service::Interface* self) {
    // TODO(Subv): Implement
#if EMU_PLATFORM == PLATFORM_WINDOWS
    WSACleanup();
    socket_blocking.clear();
#endif

    u32* cmd_buffer = Service::GetCommandBuffer();
    cmd_buffer[1] = 0;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, InitializeSockets,             "InitializeSockets"},
    {0x000200C2, Socket,                        "Socket"},
    {0x00030082, Listen,                        "Listen"},
    {0x00040082, Accept,                        "Accept"},
    {0x00050084, Bind,                          "Bind"},
    {0x00060084, Connect,                       "Connect"},
    {0x00070104, nullptr,                       "recvfrom_other"},
    {0x00080102, RecvFrom,                      "RecvFrom"},
    {0x00090106, nullptr,                       "sendto_other"},
    {0x000A0106, SendTo,                        "SendTo"},
    {0x000B0042, Close,                         "Close"},
    {0x000C0082, Shutdown,                      "Shutdown"},
    {0x000D0082, nullptr,                       "GetHostByName"},
    {0x000E00C2, nullptr,                       "GetHostByAddr"},
    {0x000F0106, nullptr,                       "unknown_resolve_ip"},
    {0x00110102, nullptr,                       "GetSockOpt"},
    {0x00120104, nullptr,                       "SetSockOpt"},
    {0x001300C2, Fcntl,                         "Fcntl"},
    {0x00140084, Poll,                          "Poll"},
    {0x00150042, nullptr,                       "SockAtMark"},
    {0x00160000, GetHostId,                     "GetHostId"},
    {0x00170082, GetSockName,                   "GetSockName"},
    {0x00180082, GetPeerName,                   "GetPeerName"},
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
#if EMU_PLATFORM == PLATFORM_WINDOWS
    socket_blocking.clear();
#endif
}

} // namespace
