// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/svc.h"

namespace Kernel {

/**
 * Signals an event
 * @param handle Handle to event to signal
 * TODO(Subv): Implement
 */
ResultCode CancelTimer(Handle handle);

/**
 * Signals an event
 * @param handle Handle to event to signal
 * TODO(Subv): Implement
 */
ResultCode SetTimer(Handle handle);

/**
 * Clears a timer
 * @param handle Handle of the timer to clear
 */
ResultCode ClearTimer(Handle handle);

/**
 * Creates a timer
 * @param Handle to newly created Timer object
 * @param reset_type ResetType describing how to create the timer
 * @param name Optional name of timer
 * @return ResultCode of the error
 */
ResultCode CreateTimer(Handle* handle, const ResetType reset_type, const std::string& name="Unknown");

} // namespace
