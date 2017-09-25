#include <memory>
#include <boost/range/algorithm/fill.hpp>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/vector_math.h"
#include "core/memory.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/regs_pipeline.h"
#include "video_core/shader/shader.h"
#include "video_core/vertex_loader.h"

namespace Pica {

MICROPROFILE_DEFINE(GPU_VertexLoad, "GPU", "Vertex Load", MP_RGB(50, 50, 240));

template <int elements, typename T>
static void LoadBufferAttr(Math::Vec4<float24>& attr, int index, u32, PAddr address) {
    const T* srcdata = reinterpret_cast<const T*>(Memory::GetPhysicalPointer(address));
    for (unsigned int comp = 0; comp < elements; ++comp) {
        attr[comp] = float24::FromFloat32(srcdata[comp]);
    }

    // Default attribute values set if array elements have < 4 components. This
    // is *not* carried over from the default attribute settings even if they're
    // enabled for this attribute.
    static constexpr std::array<float24, 4> defaults = {
        float24::Zero(), float24::Zero(), float24::Zero(), float24::FromFloat32(1.0f)};

    memcpy(&attr[elements], &defaults[elements], (defaults.size() - elements) * sizeof(float24));

    LOG_TRACE(HW_GPU, "Loaded %d components of attribute %x for vertex %x from 0x%08x: %f %f %f %f",
              elements, index, vertex, address, attr[0].ToFloat32(), attr[1].ToFloat32(),
              attr[2].ToFloat32(), attr[3].ToFloat32());
}

template <int elements>
static void LoadFloatBufferAttr(Math::Vec4<float24>& attr, int index, u32,
                                  PAddr address) {
    const float* srcdata = reinterpret_cast<const float*>(Memory::GetPhysicalPointer(address));

    // Note: We take advantage of the fact that float24 is implemented as a simple float under the
    // hood.
    static_assert(sizeof(float24) == sizeof(float), "float24 differs in size from a normal float");
    memcpy(&attr, srcdata, sizeof(float) * elements);

    // Default attribute values set if array elements have < 4 components. This
    // is *not* carried over from the default attribute settings even if they're
    // enabled for this attribute.
    static constexpr std::array<float24, 4> defaults = {
        float24::Zero(), float24::Zero(), float24::Zero(), float24::FromFloat32(1.0f)};

    memcpy(&attr[elements], &defaults[elements], (defaults.size() - elements) * sizeof(float24));
}

void LoadDefaultAttr(Math::Vec4<float24>& attr, int index, u32 elements, PAddr address) {
    // Load the default attribute if we're configured to do so
    attr = g_state.input_default_attributes.attr[index];

    LOG_TRACE(HW_GPU, "Loaded default attribute %x for vertex %x: (%f, %f, %f, %f)", index, vertex,
              attr[0].ToFloat32(), attr[1].ToFloat32(), attr[2].ToFloat32(), attr[3].ToFloat32());
}

void LoadPreviousAttr(Math::Vec4<float24>& attr, int index, u32 elements, PAddr address) {
    // TODO(yuriks): In this case, no data gets loaded and the vertex
    // remains with the last value it had. This isn't currently maintained
    // as global state, however, and so won't work in Citra yet.
}

void VertexLoader::Setup(const PipelineRegs& regs) {
    ASSERT_MSG(!is_setup, "VertexLoader is not intended to be setup more than once.");

    const auto& attribute_config = regs.vertex_attributes;
    num_total_attributes = attribute_config.GetNumTotalAttributes();

    boost::fill(vertex_attribute_sources, 0xdeadbeef);

    // Setup attribute data from loaders
    for (int loader = 0; loader < 12; ++loader) {
        const auto& loader_config = attribute_config.attribute_loaders[loader];

        u32 offset = 0;

        // TODO: What happens if a loader overwrites a previous one's data?
        for (unsigned component = 0; component < loader_config.component_count; ++component) {
            if (component >= 12) {
                LOG_ERROR(HW_GPU,
                          "Overflow in the vertex attribute loader %u trying to load component %u",
                          loader, component);
                continue;
            }

            u32 attribute_index = loader_config.GetComponent(component);
            if (attribute_index < 12) {
                offset = Common::AlignUp(offset,
                                         attribute_config.GetElementSizeInBytes(attribute_index));
                vertex_attribute_sources[attribute_index] = loader_config.data_offset + offset;
                vertex_attribute_strides[attribute_index] =
                    static_cast<u32>(loader_config.byte_count);
                vertex_attribute_formats[attribute_index] =
                    attribute_config.GetFormat(attribute_index);
                vertex_attribute_elements[attribute_index] =
                    attribute_config.GetNumElements(attribute_index);
                offset += attribute_config.GetStride(attribute_index);
            } else if (attribute_index < 16) {
                // Attribute ids 12, 13, 14 and 15 signify 4, 8, 12 and 16-byte paddings,
                // respectively
                offset = Common::AlignUp(offset, 4);
                offset += (attribute_index - 11) * 4;
            } else {
                UNREACHABLE(); // This is truly unreachable due to the number of bits for each
                               // component
            }
        }
    }

    // Set up the functions used to load the actual attributes based on their type
    for (int i = 0; i < num_total_attributes; ++i) {
        if (vertex_attribute_elements[i] != 0) {
            switch (vertex_attribute_formats[i]) {
            case PipelineRegs::VertexAttributeFormat::BYTE: {
                switch (vertex_attribute_elements[i]) {
                case 1:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<1, s8>;
                    break;
                case 2:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<2, s8>;
                    break;
                case 3:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<3, s8>;
                    break;
                case 4:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<4, s8>;
                    break;
                }
                break;
            }
            case PipelineRegs::VertexAttributeFormat::UBYTE: {
                switch (vertex_attribute_elements[i]) {
                case 1:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<1, u8>;
                    break;
                case 2:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<2, u8>;
                    break;
                case 3:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<3, u8>;
                    break;
                case 4:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<4, u8>;
                    break;
                }
                break;
            }
            case PipelineRegs::VertexAttributeFormat::SHORT: {
                switch (vertex_attribute_elements[i]) {
                case 1:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<1, s16>;
                    break;
                case 2:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<2, s16>;
                    break;
                case 3:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<3, s16>;
                    break;
                case 4:
                    vertex_attribute_loader_function[i] = LoadBufferAttr<4, s16>;
                    break;
                }
                break;
            }
            case PipelineRegs::VertexAttributeFormat::FLOAT: {
                switch (vertex_attribute_elements[i]) {
                case 1:
                    vertex_attribute_loader_function[i] = LoadFloatBufferAttr<1>;
                    break;
                case 2:
                    vertex_attribute_loader_function[i] = LoadFloatBufferAttr<2>;
                    break;
                case 3:
                    vertex_attribute_loader_function[i] = LoadFloatBufferAttr<3>;
                    break;
                case 4:
                    vertex_attribute_loader_function[i] = LoadFloatBufferAttr<4>;
                    break;
                }
                break;
            }
            }
        } else if (attribute_config.IsDefaultAttribute(i)) {
            vertex_attribute_loader_function[i] = LoadDefaultAttr;
        } else {
            vertex_attribute_loader_function[i] = LoadPreviousAttr;
        }
    }

    is_setup = true;
}

void VertexLoader::LoadVertex(u32 base_address, int index, int vertex,
                              Shader::AttributeBuffer& input,
                              DebugUtils::MemoryAccessTracker& memory_accesses) {
    MICROPROFILE_SCOPE(GPU_VertexLoad);

    ASSERT_MSG(is_setup, "A VertexLoader needs to be setup before loading vertices.");

    for (int i = 0; i < num_total_attributes; ++i) {
        // Load per-vertex data from the loader arrays or the default attributes array
        u32 source_addr =
            base_address + vertex_attribute_sources[i] + vertex_attribute_strides[i] * vertex;
        vertex_attribute_loader_function[i](input.attr[i], i, vertex_attribute_elements[i],
                                            source_addr);
    }
}

} // namespace Pica
