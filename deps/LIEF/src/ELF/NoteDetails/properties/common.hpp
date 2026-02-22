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
#ifndef LIEF_ELF_NOTE_GNU_PROPERTIES_COMMON_H
#define LIEF_ELF_NOTE_GNU_PROPERTIES_COMMON_H

#include <cstdint>

namespace LIEF::ELF {

static constexpr auto GNU_PROPERTY_LOPROC = 0xc0000000;
static constexpr auto GNU_PROPERTY_HIPROC = 0xdfffffff;
static constexpr auto GNU_PROPERTY_LOUSER = 0xe0000000;
static constexpr auto GNU_PROPERTY_HIUSER = 0xffffffff;
static constexpr auto GNU_PROPERTY_AARCH64_FEATURE_1_AND = 0xc0000000;
static constexpr auto GNU_PROPERTY_AARCH64_FEATURE_PAUTH = 0xc0000001;

static constexpr auto GNU_PROPERTY_STACK_SIZE = 1;
static constexpr auto GNU_PROPERTY_NO_COPY_ON_PROTECTED = 2;

static constexpr auto GNU_PROPERTY_UINT32_AND_LO = 0xb0000000;
static constexpr auto GNU_PROPERTY_UINT32_AND_HI = 0xb0007fff;

static constexpr auto GNU_PROPERTY_UINT32_OR_LO = 0xb0008000;
static constexpr auto GNU_PROPERTY_UINT32_OR_HI = 0xb000ffff;

static constexpr auto GNU_PROPERTY_1_NEEDED = GNU_PROPERTY_UINT32_OR_LO;

static constexpr auto GNU_PROPERTY_X86_UINT32_OR_AND_LO = 0xc0010000;
static constexpr auto GNU_PROPERTY_X86_UINT32_OR_AND_HI = 0xc0017fff;
static constexpr auto GNU_PROPERTY_X86_UINT32_OR_LO = 0xc0008000;
static constexpr auto GNU_PROPERTY_X86_UINT32_OR_HI = 0xc000ffff;
static constexpr auto GNU_PROPERTY_X86_UINT32_AND_LO = 0xc0000002;
static constexpr auto GNU_PROPERTY_X86_UINT32_AND_HI = 0xc0007fff;

static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_USED = GNU_PROPERTY_X86_UINT32_OR_AND_LO + 0;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_USED = GNU_PROPERTY_X86_UINT32_OR_AND_LO + 1;
static constexpr auto GNU_PROPERTY_X86_ISA_1_USED = GNU_PROPERTY_X86_UINT32_OR_AND_LO + 2;

static constexpr auto GNU_PROPERTY_X86_FEATURE_1_AND = GNU_PROPERTY_X86_UINT32_AND_LO + 0;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_USED = 0xc0000000;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_NEEDED = 0xc0000001;

static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_NEEDED = GNU_PROPERTY_X86_UINT32_OR_LO + 0;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_NEEDED = GNU_PROPERTY_X86_UINT32_OR_LO + 1;
static constexpr auto GNU_PROPERTY_X86_ISA_1_NEEDED = GNU_PROPERTY_X86_UINT32_OR_LO + 2;

static constexpr auto GNU_PROPERTY_X86_ISA_1_BASELINE = 1U << 0;
static constexpr auto GNU_PROPERTY_X86_ISA_1_V2 = 1U << 1;
static constexpr auto GNU_PROPERTY_X86_ISA_1_V3 = 1U << 2;
static constexpr auto GNU_PROPERTY_X86_ISA_1_V4 = 1U << 3;

static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_486      = 1U << 0;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_586      = 1U << 1;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_686      = 1U << 2;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_SSE      = 1U << 3;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_SSE2     = 1U << 4;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_SSE3     = 1U << 5;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_SSSE3    = 1U << 6;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_SSE4_1   = 1U << 7;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_SSE4_2   = 1U << 8;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX      = 1U << 9;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX2     = 1U << 10;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512F  = 1U << 11;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512CD = 1U << 12;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512ER = 1U << 13;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512PF = 1U << 14;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512VL = 1U << 15;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512DQ = 1U << 16;
static constexpr auto GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512BW = 1U << 17;

static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_CMOV           = 1U << 0;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE            = 1U << 1;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE2           = 1U << 2;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE3           = 1U << 3;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSSE3          = 1U << 4;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE4_1         = 1U << 5;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE4_2         = 1U << 6;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX            = 1U << 7;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX2           = 1U << 8;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_FMA            = 1U << 9;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512F        = 1U << 10;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512CD       = 1U << 11;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512ER       = 1U << 12;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512PF       = 1U << 13;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512VL       = 1U << 14;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512DQ       = 1U << 15;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512BW       = 1U << 16;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_4FMAPS  = 1U << 17;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_4VNNIW  = 1U << 18;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_BITALG  = 1U << 19;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_IFMA    = 1U << 20;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_VBMI    = 1U << 21;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_VBMI2   = 1U << 22;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_VNNI    = 1U << 23;
static constexpr auto GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_BF16    = 1U << 24;


static constexpr auto GNU_PROPERTY_X86_FEATURE_1_IBT     = 1U << 0;
static constexpr auto GNU_PROPERTY_X86_FEATURE_1_SHSTK   = 1U << 1;
static constexpr auto GNU_PROPERTY_X86_FEATURE_1_LAM_U48 = 1U << 2;
static constexpr auto GNU_PROPERTY_X86_FEATURE_1_LAM_U57 = 1U << 3;

static constexpr auto GNU_PROPERTY_X86_FEATURE_2_X86      = 1U << 0;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_X87      = 1U << 1;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_MMX      = 1U << 2;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_XMM      = 1U << 3;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_YMM      = 1U << 4;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_ZMM      = 1U << 5;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_FXSR     = 1U << 6;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_XSAVE    = 1U << 7;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_XSAVEOPT = 1U << 8;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_XSAVEC   = 1U << 9;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_TMM      = 1U << 10;
static constexpr auto GNU_PROPERTY_X86_FEATURE_2_MASK     = 1U << 11;

}

#endif
