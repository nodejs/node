//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// NOTE: This file is intended to be "#include" multiple times. The call site must define the macros
// "LAYOUT_TYPE", etc., to be executed for each entry.
//

#ifndef LAYOUT_TYPE
#define LAYOUT_TYPE(layout)
#endif

#ifndef LAYOUT_TYPE_WMS
#define LAYOUT_TYPE_WMS(layout) \
    LAYOUT_TYPE(layout##_Small) \
    LAYOUT_TYPE(layout##_Medium) \
    LAYOUT_TYPE(layout##_Large)
#endif

#ifndef LAYOUT_TYPE_PROFILED
#define LAYOUT_TYPE_PROFILED(layout) \
    LAYOUT_TYPE(layout) \
    LAYOUT_TYPE(Profiled##layout)
#endif

#ifndef LAYOUT_TYPE_PROFILED2
#define LAYOUT_TYPE_PROFILED2(layout) \
    LAYOUT_TYPE_PROFILED(layout) \
    LAYOUT_TYPE(Profiled2##layout)
#endif

#ifndef LAYOUT_TYPE_PROFILED_WMS
#define LAYOUT_TYPE_PROFILED_WMS(layout) \
    LAYOUT_TYPE_WMS(layout) \
    LAYOUT_TYPE_WMS(Profiled##layout)
#endif

#ifndef LAYOUT_TYPE_PROFILED2_WMS
#define LAYOUT_TYPE_PROFILED2_WMS(layout) \
    LAYOUT_TYPE_PROFILED_WMS(layout) \
    LAYOUT_TYPE_WMS(Profiled2##layout)
#endif

LAYOUT_TYPE                 (Empty)
LAYOUT_TYPE_WMS             (Reg1)
LAYOUT_TYPE_PROFILED_WMS    (Reg2)
LAYOUT_TYPE_PROFILED_WMS    (Reg2WithICIndex)
LAYOUT_TYPE_PROFILED_WMS    (Reg3)
LAYOUT_TYPE_WMS             (Reg4)
LAYOUT_TYPE_PROFILED_WMS    (Reg1Unsigned1)
LAYOUT_TYPE_WMS             (Reg2B1)
LAYOUT_TYPE_WMS             (Reg3B1)
LAYOUT_TYPE_WMS             (Reg3C)
LAYOUT_TYPE_PROFILED_WMS    (Arg)
LAYOUT_TYPE_WMS             (ArgNoSrc)
#ifdef BYTECODE_BRANCH_ISLAND
LAYOUT_TYPE                 (BrLong)
#endif
LAYOUT_TYPE                 (Br)
LAYOUT_TYPE_WMS             (BrReg1)
LAYOUT_TYPE_WMS             (BrReg2)
LAYOUT_TYPE                 (BrS)
LAYOUT_TYPE                 (BrProperty)
LAYOUT_TYPE                 (BrLocalProperty)
LAYOUT_TYPE                 (BrEnvProperty)
LAYOUT_TYPE                 (StartCall)
LAYOUT_TYPE_PROFILED2_WMS   (CallI)
LAYOUT_TYPE_PROFILED_WMS    (CallIFlags)
LAYOUT_TYPE_PROFILED_WMS    (CallIWithICIndex)
LAYOUT_TYPE_PROFILED_WMS    (CallIFlagsWithICIndex)
LAYOUT_TYPE_PROFILED2_WMS   (CallIExtended)
LAYOUT_TYPE_PROFILED_WMS    (CallIExtendedFlags)
LAYOUT_TYPE_PROFILED_WMS    (CallIExtendedWithICIndex)
LAYOUT_TYPE_PROFILED_WMS    (CallIExtendedFlagsWithICIndex)
LAYOUT_TYPE_WMS             (Class)
LAYOUT_TYPE_PROFILED_WMS    (ElementI)
LAYOUT_TYPE_WMS             (ElementUnsigned1)
LAYOUT_TYPE_WMS             (ElementC)
LAYOUT_TYPE_WMS             (ElementScopedC)
LAYOUT_TYPE_WMS             (ElementCP)
LAYOUT_TYPE_WMS             (ElementP)
LAYOUT_TYPE_WMS             (ElementPIndexed)
LAYOUT_TYPE_WMS             (ElementRootCP)
LAYOUT_TYPE_WMS             (ElementC2)
LAYOUT_TYPE_WMS             (ElementScopedC2)
LAYOUT_TYPE_PROFILED_WMS    (ElementSlot)
LAYOUT_TYPE_PROFILED_WMS    (ElementSlotI1)
LAYOUT_TYPE_PROFILED_WMS    (ElementSlotI2)
LAYOUT_TYPE_WMS             (ElementU)
LAYOUT_TYPE_WMS             (ElementScopedU)
LAYOUT_TYPE_WMS             (ElementRootU)
LAYOUT_TYPE                 (W1)
LAYOUT_TYPE                 (Reg1Int2)
LAYOUT_TYPE_WMS             (Reg2Int1)
LAYOUT_TYPE                 (AuxNoReg)
LAYOUT_TYPE_PROFILED        (Auxiliary)
LAYOUT_TYPE                 (Reg2Aux)
LAYOUT_TYPE_WMS             (Reg5)
LAYOUT_TYPE_WMS             (Unsigned1)

#undef LAYOUT_TYPE
#undef LAYOUT_TYPE_WMS
#undef LAYOUT_TYPE_PROFILED2_WMS
#undef LAYOUT_TYPE_PROFILED_WMS
#undef LAYOUT_TYPE_PROFILED
#undef LAYOUT_TYPE_PROFILED2
