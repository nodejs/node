//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// NOTE: This file is intended to be "#include" multiple times. The call site must define the macros
// "LAYOUT_TYPE", etc., to be executed for each entry.
//

#ifndef TEMP_DISABLE_ASMJS
#ifndef LAYOUT_TYPE
#define LAYOUT_TYPE(layout)
#endif

#ifndef LAYOUT_TYPE_WMS
#define LAYOUT_TYPE_WMS(layout) \
    LAYOUT_TYPE(layout##_Small) \
    LAYOUT_TYPE(layout##_Medium) \
    LAYOUT_TYPE(layout##_Large)
#endif

// FE for frontend only layout
#ifdef EXCLUDE_FRONTEND_LAYOUT
#ifndef LAYOUT_TYPE_WMS_FE
#define LAYOUT_TYPE_WMS_FE(...)
#endif
#else
#ifndef LAYOUT_TYPE_WMS_FE
#define LAYOUT_TYPE_WMS_FE LAYOUT_TYPE_WMS
#endif
#endif

// For duplicates layout from LayoutTypes.h
#ifdef EXCLUDE_DUP_LAYOUT
#define LAYOUT_TYPE_DUP(...)
#define LAYOUT_TYPE_WMS_DUP(...)
#else
#define LAYOUT_TYPE_DUP LAYOUT_TYPE
#define LAYOUT_TYPE_WMS_DUP LAYOUT_TYPE_WMS
#endif

// These layout are already defined in LayoutTypes.h
// We redeclare them here to keep the same layout and use them
// in other contexts.
LAYOUT_TYPE_WMS_DUP ( ElementSlot      )
LAYOUT_TYPE_DUP     ( StartCall        )
LAYOUT_TYPE_DUP     ( Empty            )

LAYOUT_TYPE_WMS     ( AsmTypedArr      )
LAYOUT_TYPE_WMS     ( AsmCall          )
LAYOUT_TYPE         ( AsmBr            )
LAYOUT_TYPE_WMS     ( AsmReg1          ) // Generic layout with 1 RegSlot
LAYOUT_TYPE_WMS_FE  ( AsmReg2          ) // Generic layout with 2 RegSlot
LAYOUT_TYPE_WMS_FE  ( AsmReg3          ) // Generic layout with 3 RegSlot
LAYOUT_TYPE_WMS_FE  ( AsmReg4          ) // Generic layout with 4 RegSlot
LAYOUT_TYPE_WMS_FE  ( AsmReg5          ) // Generic layout with 5 RegSlot
LAYOUT_TYPE_WMS_FE  ( AsmReg6          ) // Generic layout with 6 RegSlot
LAYOUT_TYPE_WMS_FE  ( AsmReg7          ) // Generic layout with 7 RegSlot
LAYOUT_TYPE_WMS_FE  ( AsmReg2IntConst1 ) // Generic layout with 2 RegSlots and 1 Int Constant
LAYOUT_TYPE_WMS     ( Int1Double1      ) // 1 int register and 1 double register
LAYOUT_TYPE_WMS     ( Int1Float1       ) // 1 int register and 1 float register
LAYOUT_TYPE_WMS     ( Double1Int1      ) // 1 double register and 1 int register
LAYOUT_TYPE_WMS     ( Double1Float1    ) // 1 double register and 1 float register
LAYOUT_TYPE_WMS     ( Double1Reg1      ) // 1 double register and 1 var register
LAYOUT_TYPE_WMS     ( Float1Reg1       ) // 1 double register and 1 var register
LAYOUT_TYPE_WMS     ( Int1Reg1         ) // 1 int register and 1 var register
LAYOUT_TYPE_WMS     ( Reg1Double1      ) // 1 var register and 1 double register
LAYOUT_TYPE_WMS     ( Reg1Float1       ) // 1 var register and 1 Float register
LAYOUT_TYPE_WMS     ( Reg1Int1         ) // 1 var register and 1 int register
LAYOUT_TYPE_WMS     ( Int1Const1       ) // 1 int register and 1 const int value
LAYOUT_TYPE_WMS     ( Int1Double2      ) // 1 int register and 2 double register ( double comparisons )
LAYOUT_TYPE_WMS     ( Int1Float2       ) // 1 int register and 2 float register ( float comparisons )
LAYOUT_TYPE_WMS     ( Int2             ) // 2 int register
LAYOUT_TYPE_WMS     ( Int3             ) // 3 int register
LAYOUT_TYPE_WMS     ( Double2          ) // 2 double register
LAYOUT_TYPE_WMS     ( Float2           ) // 2 float register
LAYOUT_TYPE_WMS     ( Float3           ) // 3 float register
LAYOUT_TYPE_WMS     ( Float1Double1    ) // 2 double register
LAYOUT_TYPE_WMS     ( Float1Int1       ) // 2 double register
LAYOUT_TYPE_WMS     ( Double3          ) // 3 double register
LAYOUT_TYPE_WMS     ( BrInt1           ) // Conditional branching with 1 int
LAYOUT_TYPE_WMS     ( BrInt2           ) // Conditional branching with 2 int
LAYOUT_TYPE_WMS     ( AsmUnsigned1     ) // Conditional branching with 2 int

// Float32x4
LAYOUT_TYPE_WMS     ( Float32x4_2 )
LAYOUT_TYPE_WMS     ( Float32x4_3 )
LAYOUT_TYPE_WMS     ( Float32x4_4 )
LAYOUT_TYPE_WMS     ( Float32x4_1Float4 )
LAYOUT_TYPE_WMS     ( Float32x4_2Int4 )
LAYOUT_TYPE_WMS     ( Float32x4_3Int4 )
LAYOUT_TYPE_WMS     ( Float32x4_1Float1 )
LAYOUT_TYPE_WMS     ( Float32x4_2Float1 )
LAYOUT_TYPE_WMS     ( Float32x4_1Float64x2_1 )
LAYOUT_TYPE_WMS     ( Float32x4_1Int32x4_1 )
LAYOUT_TYPE_WMS     ( Float32x4_1Int32x4_1Float32x4_2 )
LAYOUT_TYPE_WMS     ( Reg1Float32x4_1 )
LAYOUT_TYPE_WMS     ( Float1Float32x4_1Int1 )
LAYOUT_TYPE_WMS     ( Float32x4_2Int1Float1 )
LAYOUT_TYPE_WMS     ( Int1Float32x4_1)
// Int32x4
LAYOUT_TYPE_WMS     ( Int32x4_2)
LAYOUT_TYPE_WMS     ( Int32x4_3)
LAYOUT_TYPE_WMS     ( Int32x4_4)
LAYOUT_TYPE_WMS     ( Int32x4_1Int4 )
LAYOUT_TYPE_WMS     ( Int32x4_2Int4 )
LAYOUT_TYPE_WMS     ( Int32x4_3Int4 )
LAYOUT_TYPE_WMS     ( Int32x4_1Int1 )
LAYOUT_TYPE_WMS     ( Int32x4_2Int1 )
LAYOUT_TYPE_WMS     ( Reg1Int32x4_1 )
LAYOUT_TYPE_WMS     ( Int32x4_1Float32x4_1 )
LAYOUT_TYPE_WMS     ( Int32x4_1Float64x2_1 )
LAYOUT_TYPE_WMS     ( Int1Int32x4_1)
LAYOUT_TYPE_WMS     ( Int1Int32x4_1Int1 )
LAYOUT_TYPE_WMS     ( Int32x4_2Int2 )
// Float64x2
LAYOUT_TYPE_WMS     ( Float64x2_2 )
LAYOUT_TYPE_WMS     ( Float64x2_3 )
LAYOUT_TYPE_WMS     ( Float64x2_4 )
LAYOUT_TYPE_WMS     ( Float64x2_1Double2 )
LAYOUT_TYPE_WMS     ( Float64x2_1Double1 )
LAYOUT_TYPE_WMS     ( Float64x2_2Double1 )
LAYOUT_TYPE_WMS     ( Float64x2_2Int2 )
LAYOUT_TYPE_WMS     ( Float64x2_3Int2 )
LAYOUT_TYPE_WMS     ( Float64x2_1Float32x4_1 )
LAYOUT_TYPE_WMS     ( Float64x2_1Int32x4_1 )
LAYOUT_TYPE_WMS     ( Float64x2_1Int32x4_1Float64x2_2 )
LAYOUT_TYPE_WMS     ( Reg1Float64x2_1 )
LAYOUT_TYPE_WMS     ( Int1Float64x2_1)
LAYOUT_TYPE_WMS     ( AsmSimdTypedArr )

#undef LAYOUT_TYPE_DUP
#undef LAYOUT_TYPE_WMS_DUP
#undef LAYOUT_TYPE
#undef LAYOUT_TYPE_WMS
#undef EXCLUDE_DUP_LAYOUT
#undef LAYOUT_TYPE_WMS_FE
#undef EXCLUDE_FRONTEND_LAYOUT
#endif
