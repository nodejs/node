//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{
#if DBG_DUMP || ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t const * const OpCodeUtilAsmJs::OpCodeAsmJsNames[] =
    {
#define DEF_OP(x, y, ...) L"" STRINGIZEW(x) L"",
#include "OpCodeListAsmJs.h"
#undef DEF_OP
    };

    wchar_t const * const OpCodeUtilAsmJs::ExtendedOpCodeAsmJsNames[] =
    {
#define DEF_OP(x, y, ...) L"" STRINGIZEW(x) L"",
#include "ExtendedOpCodeListAsmJs.h"
#undef DEF_OP
    };

    wchar_t const * OpCodeUtilAsmJs::GetOpCodeName(OpCodeAsmJs op)
    {
        if (op <= Js::OpCodeAsmJs::MaxByteSizedOpcodes)
        {
            Assert(op < _countof(OpCodeAsmJsNames));
            __analysis_assume(op < _countof(OpCodeAsmJsNames));
            return OpCodeAsmJsNames[(int)op];
        }
        else if (op < Js::OpCodeAsmJs::ByteCodeLast)
        {
            uint opIndex = op - (Js::OpCodeAsmJs::MaxByteSizedOpcodes + 1);
            Assert(opIndex < _countof(ExtendedOpCodeAsmJsNames));
            __analysis_assume(opIndex < _countof(ExtendedOpCodeAsmJsNames));
            return ExtendedOpCodeAsmJsNames[opIndex];
        }
        return L"<NotAvail>";
    }

#else
    wchar const * OpCodeUtilAsmJs::GetOpCodeName(OpCodeAsmJs op)
    {
        return L"<NotAvail>";
    }
#endif

    OpLayoutTypeAsmJs const OpCodeUtilAsmJs::OpCodeAsmJsLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutTypeAsmJs::y,
#include "OpCodeListAsmJs.h"
    };

    OpLayoutTypeAsmJs const OpCodeUtilAsmJs::ExtendedOpCodeAsmJsLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutTypeAsmJs::y,
#include "ExtendedOpCodeListAsmJs.h"
    };

    OpLayoutTypeAsmJs OpCodeUtilAsmJs::GetOpCodeLayout(OpCodeAsmJs op)
    {
        if ((uint)op <= (uint)Js::OpCodeAsmJs::MaxByteSizedOpcodes)
        {
            Assert(op < _countof(OpCodeAsmJsLayouts));
            __analysis_assume(op < _countof(OpCodeAsmJsLayouts));
            return OpCodeAsmJsLayouts[(uint)op];
        }

        uint opIndex = op - (Js::OpCodeAsmJs::MaxByteSizedOpcodes + 1);
        Assert(opIndex < _countof(ExtendedOpCodeAsmJsLayouts));
        __analysis_assume(opIndex < _countof(ExtendedOpCodeAsmJsLayouts));
        return ExtendedOpCodeAsmJsLayouts[opIndex];
    }

    bool OpCodeUtilAsmJs::IsValidByteCodeOpcode(OpCodeAsmJs op)
    {
        CompileAssert((int)Js::OpCodeAsmJs::MaxByteSizedOpcodes + 1 + _countof(OpCodeUtilAsmJs::ExtendedOpCodeAsmJsLayouts) == (int)Js::OpCodeAsmJs::ByteCodeLast);
        return op < _countof(OpCodeAsmJsLayouts)
            || (op > Js::OpCodeAsmJs::MaxByteSizedOpcodes && op < Js::OpCodeAsmJs::ByteCodeLast);
    }

    bool OpCodeUtilAsmJs::IsValidOpcode(OpCodeAsmJs op)
    {
        return IsValidByteCodeOpcode(op)
            || (op > Js::OpCodeAsmJs::ByteCodeLast && op < Js::OpCodeAsmJs::Count);
    }
};
#endif
