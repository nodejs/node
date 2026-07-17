/* Copyright 2021 - 2025 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PE_STRUCTURES_H
#define LIEF_PE_STRUCTURES_H
#include <type_traits>
#include <map>

#include "LIEF/types.hpp"

#include "LIEF/PE/enums.hpp"

namespace LIEF {

/// Namespace related to the LIEF's PE module
///
/// Some parts come from llvm/Support/COFF.h
namespace PE {

namespace details {

/// Sizes in bytes of various things in the COFF format.
namespace sizes {
  static constexpr size_t HEADER_16                    = 20;
  static constexpr size_t HEADER_32                    = 56;
  static constexpr size_t SECTION_NAME                 = 8;
  static constexpr size_t SYMBOL_16                    = 18;
  static constexpr size_t SYMBOL_32                    = 20;
  static constexpr size_t SECTION                      = 40;
  static constexpr size_t RELOCATION                   = 10;
  static constexpr size_t BASE_RELOCATION_BLOCK        = 8;
  static constexpr size_t IMPORT_DIRECTORY_TABLE_ENTRY = 20;
  static constexpr size_t RESOURCE_DIRECTORY_TABLE     = 16;
  static constexpr size_t RESOURCE_DIRECTORY_ENTRIES   = 8;
  static constexpr size_t RESOURCE_DATA_ENTRY          = 16;
}

struct delay_imports {
  uint32_t attribute;
  uint32_t name;
  uint32_t handle;
  uint32_t iat;
  uint32_t name_table;
  uint32_t bound_iat;
  uint32_t unload_iat;
  uint32_t timestamp;
};

static_assert(sizeof(delay_imports) == 32, "Wrong sizeof(delay_imports)");

#include "structures.inc"

// From Virtualbox - include/iprt/formats/pecoff.h
template <typename T>
struct load_configuration {
  uint32_t Characteristics;
  uint32_t TimeDateStamp;
  uint16_t MajorVersion;
  uint16_t MinorVersion;
  uint32_t GlobalFlagsClear;
  uint32_t GlobalFlagsSet;
  uint32_t CriticalSectionDefaultTimeout;
  T        DeCommitFreeBlockThreshold;
  T        DeCommitTotalFreeThreshold;
  T        LockPrefixTable;
  T        MaximumAllocationSize;
  T        VirtualMemoryThreshold;
  T        ProcessAffinityMask;
  uint32_t ProcessHeapFlags;
  uint16_t CSDVersion;
  uint16_t Reserved1;
  T        EditList;
  T        SecurityCookie;
};

template <typename T>
struct load_configuration_v0 : load_configuration<T> {
  T SEHandlerTable;
  T SEHandlerCount;
};


#pragma pack(4)
// Windows 10 - 9879

template <typename T>
struct load_configuration_v1 : load_configuration_v0<T> {
  T        GuardCFCheckFunctionPointer;
  T        GuardCFDispatchFunctionPointer;
  T        GuardCFFunctionTable;
  T        GuardCFFunctionCount;
  uint32_t GuardFlags;
};
#pragma pack()


// Windows 10 - 9879
template <typename T>
struct load_configuration_v2 : load_configuration_v1<T> {
  pe_code_integrity CodeIntegrity;
};


template <typename T>
struct load_configuration_v3 : load_configuration_v2<T> {
  T GuardAddressTakenIatEntryTable;
  T GuardAddressTakenIatEntryCount;
  T GuardLongJumpTargetTable;
  T GuardLongJumpTargetCount;
};


template <typename T>
struct load_configuration_v4 : load_configuration_v3<T> {
  T DynamicValueRelocTable;
  T HybridMetadataPointer;
};


template <typename T>
struct load_configuration_v5 : load_configuration_v4<T> {
  T        GuardRFFailureRoutine;
  T        GuardRFFailureRoutineFunctionPointer;
  uint32_t DynamicValueRelocTableOffset;
  uint16_t DynamicValueRelocTableSection;
  uint16_t Reserved2;
};


#pragma pack(4)
template <typename T>
struct load_configuration_v6 : load_configuration_v5<T> {
  T        GuardRFVerifyStackPointerFunctionPointer;
  uint32_t HotPatchTableOffset;
};
#pragma pack()

template <typename T>
struct load_configuration_v7 : load_configuration_v6<T> {
  uint32_t Reserved3;
  T        AddressOfSomeUnicodeString;
};

template <typename T>
struct load_configuration_v8 : load_configuration_v7<T> {
  T VolatileMetadataPointer;
};

template <typename T>
struct load_configuration_v9 : load_configuration_v8<T> {
  T GuardEHContinuationTable;
  T GuardEHContinuationCount;
};

template <typename T>
struct load_configuration_v10 : load_configuration_v9<T> {
  T GuardXFGCheckFunctionPointer;
  T GuardXFGDispatchFunctionPointer;
  T GuardXFGTableDispatchFunctionPointer;
};

template <typename T>
struct load_configuration_v11 : load_configuration_v10<T> {
  T CastGuardOsDeterminedFailureMode;
};


class PE32 {
  public:
    using pe_optional_header = pe32_optional_header;
    using pe_tls             = pe32_tls;
    using uint               = uint32_t;

    using load_configuration_t     = load_configuration<uint32_t>;
    using load_configuration_v0_t  = load_configuration_v0<uint32_t>;
    using load_configuration_v1_t  = load_configuration_v1<uint32_t>;
    using load_configuration_v2_t  = load_configuration_v2<uint32_t>;
    using load_configuration_v3_t  = load_configuration_v3<uint32_t>;
    using load_configuration_v4_t  = load_configuration_v4<uint32_t>;
    using load_configuration_v5_t  = load_configuration_v5<uint32_t>;
    using load_configuration_v6_t  = load_configuration_v6<uint32_t>;
    using load_configuration_v7_t  = load_configuration_v7<uint32_t>;
    using load_configuration_v8_t  = load_configuration_v8<uint32_t>;
    using load_configuration_v9_t  = load_configuration_v9<uint32_t>;
    using load_configuration_v10_t = load_configuration_v10<uint32_t>;
    using load_configuration_v11_t = load_configuration_v11<uint32_t>;

    static_assert(sizeof(load_configuration_t)     == 0x40);
    static_assert(sizeof(load_configuration_v0_t)  == 0x48);
    static_assert(sizeof(load_configuration_v1_t)  == 0x5c);
    static_assert(sizeof(load_configuration_v2_t)  == 0x68);
    static_assert(sizeof(load_configuration_v3_t)  == 0x78);
    static_assert(sizeof(load_configuration_v4_t)  == 0x80);
    static_assert(sizeof(load_configuration_v5_t)  == 0x90);
    static_assert(sizeof(load_configuration_v6_t)  == 0x98);
    static_assert(sizeof(load_configuration_v7_t)  == 0xA0);
    static_assert(sizeof(load_configuration_v8_t)  == 0xA4);
    static_assert(sizeof(load_configuration_v9_t)  == 0xAC);
    static_assert(sizeof(load_configuration_v10_t) == 0xB8);
    static_assert(sizeof(load_configuration_v11_t) == 0xBC);
 };


class PE64 {
  public:
    using pe_optional_header = pe64_optional_header;
    using pe_tls             = pe64_tls;
    using uint               = uint64_t;

    using load_configuration_t     = load_configuration<uint64_t>;
    using load_configuration_v0_t  = load_configuration_v0<uint64_t>;
    using load_configuration_v1_t  = load_configuration_v1<uint64_t>;
    using load_configuration_v2_t  = load_configuration_v2<uint64_t>;
    using load_configuration_v3_t  = load_configuration_v3<uint64_t>;
    using load_configuration_v4_t  = load_configuration_v4<uint64_t>;
    using load_configuration_v5_t  = load_configuration_v5<uint64_t>;
    using load_configuration_v6_t  = load_configuration_v6<uint64_t>;
    using load_configuration_v7_t  = load_configuration_v7<uint64_t>;
    using load_configuration_v8_t  = load_configuration_v8<uint64_t>;
    using load_configuration_v9_t  = load_configuration_v9<uint64_t>;
    using load_configuration_v10_t = load_configuration_v10<uint64_t>;
    using load_configuration_v11_t = load_configuration_v11<uint64_t>;

    static_assert(sizeof(load_configuration_t)     == 0x60);
    static_assert(sizeof(load_configuration_v0_t)  == 0x70);
    static_assert(sizeof(load_configuration_v1_t)  == 0x94);
    static_assert(sizeof(load_configuration_v2_t)  == 0xA0);
    static_assert(sizeof(load_configuration_v3_t)  == 0xC0);
    static_assert(sizeof(load_configuration_v4_t)  == 0xD0);
    static_assert(sizeof(load_configuration_v5_t)  == 0xE8);
    static_assert(sizeof(load_configuration_v6_t)  == 0xF4);
    static_assert(sizeof(load_configuration_v7_t)  == 0x100);
    static_assert(sizeof(load_configuration_v8_t)  == 0x108);
    static_assert(sizeof(load_configuration_v9_t)  == 0x118);
    static_assert(sizeof(load_configuration_v10_t) == 0x130);
    static_assert(sizeof(load_configuration_v11_t) == 0x138);
};
}


} // end namesapce PE
}

#endif
