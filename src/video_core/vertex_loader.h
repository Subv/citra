#pragma once

#include <array>
#include "common/common_types.h"
#include "video_core/regs_pipeline.h"

namespace Pica {

namespace DebugUtils {
class MemoryAccessTracker;
}

namespace Shader {
struct AttributeBuffer;
}

class VertexLoader {
public:
    VertexLoader() = default;
    explicit VertexLoader(const PipelineRegs& regs) {
        Setup(regs);
    }

    void Setup(const PipelineRegs& regs);
    void LoadVertex(u32 base_address, int index, int vertex, Shader::AttributeBuffer& input,
                    DebugUtils::MemoryAccessTracker& memory_accesses);

    int GetNumTotalAttributes() const {
        return num_total_attributes;
    }

private:
    using LoaderFunction = void(Math::Vec4<float24>& attr, int index, u32 elements, PAddr address);
    std::array<LoaderFunction*, 16> vertex_attribute_loader_function;
    std::array<u32, 16> vertex_attribute_sources;
    std::array<u32, 16> vertex_attribute_strides{};
    std::array<PipelineRegs::VertexAttributeFormat, 16> vertex_attribute_formats;
    std::array<u32, 16> vertex_attribute_elements{};
    int num_total_attributes = 0;
    bool is_setup = false;
};

} // namespace Pica
