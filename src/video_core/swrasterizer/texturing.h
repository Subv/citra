// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/regs_texturing.h"

namespace Pica {
namespace Rasterizer {

int GetWrappedTexCoord(TexturingRegs::TextureConfig::WrapMode mode, int val, unsigned size);

using ColorModifierFunc = Math::Vec3<u8>(*)(const Math::Vec4<u8>&);
ColorModifierFunc ConfigureColorModifier(TexturingRegs::TevStageConfig::ColorModifier factor);

u8 GetAlphaModifier(TexturingRegs::TevStageConfig::AlphaModifier factor,
                    const Math::Vec4<u8>& values);

Math::Vec3<u8> ColorCombine(TexturingRegs::TevStageConfig::Operation op,
                            const Math::Vec3<u8> input[3]);

using AlphaCombineFunc = u8(*)(const std::array<u8, 3>&);
AlphaCombineFunc ConfigureAlphaCombine(TexturingRegs::TevStageConfig::Operation op);

} // namespace Rasterizer
} // namespace Pica
