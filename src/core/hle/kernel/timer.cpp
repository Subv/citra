// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Timer : public Object {
public:
    std::string GetTypeName() const override { return "Timer"; }
    std::string GetName() const override { return name; }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Timer; }
    Kernel::HandleType GetHandleType() const override { return Kernel::HandleType::Timer; }

    ResetType reset_type;                   ///< The ResetType of this timer

    bool signaled;                          ///< Whether the timer has been signaled or not
    std::vector<Handle> waiting_threads;    ///< Threads that are waiting for the timer
    std::string name;                       ///< Name of timer (optional)

    ResultVal<bool> WaitSynchronization() override {
        bool wait = !signaled;
        if (wait) {
            waiting_threads.push_back(GetCurrentThreadHandle());
            Kernel::WaitCurrentThread(WAITTYPE_TIMER, GetHandle());
        } else {
            if (reset_type != RESETTYPE_STICKY)
                signaled = false;
        }
        return MakeResult<bool>(wait);
    }
};

/**
 * Creates a timer.
 * @param handle Reference to handle for the newly created timer
 * @param reset_type ResetType describing how to create timer
 * @param name Optional name of timer
 * @return Newly created Timer object
 */
Timer* CreateTimer(Handle& handle, const ResetType reset_type, const std::string& name) {
    Timer* timer = new Timer;

    handle = Kernel::g_object_pool.Create(timer);

    timer->reset_type = reset_type;
    timer->signaled = false;
    timer->name = name;

    return timer;
}

ResultCode CreateTimer(Handle* handle, const ResetType reset_type, const std::string& name) {
    CreateTimer(*handle, reset_type, name);
    return RESULT_SUCCESS;
}

ResultCode ClearTimer(Handle handle) {
    Timer* timer = Kernel::g_object_pool.Get<Timer>(handle);
    
    if (timer == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    timer->signaled = false;
    return RESULT_SUCCESS;
}

} // namespace
