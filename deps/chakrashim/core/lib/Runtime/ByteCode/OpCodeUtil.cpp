//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    bool OpCodeUtil::IsPrefixOpcode(OpCode op)
    {
        return op <= OpCode::ExtendedLargeLayoutPrefix && op != OpCode::EndOfBlock;
    }

    bool OpCodeUtil::IsSmallEncodedOpcode(OpCode op)
    {
        return op <= Js::OpCode::MaxByteSizedOpcodes;
    }
    uint OpCodeUtil::EncodedSize(OpCode op, LayoutSize layoutSize)
    {
        return (layoutSize == SmallLayout && IsSmallEncodedOpcode(op)) ? sizeof(BYTE) : sizeof(OpCode);
    }

    void OpCodeUtil::ConvertOpToNonProfiled(OpCode& op)
    {
        if (IsProfiledCallOp(op) || IsProfiledCallOpWithICIndex(op))
        {
            op = ConvertProfiledCallOpToNonProfiled(op);
        }
        else if (IsProfiledReturnTypeCallOp(op))
        {
            op = ConvertProfiledReturnTypeCallOpToNonProfiled(op);
        }
        else
        {
            ConvertNonCallOpToNonProfiled(op);
        }
    }
    void OpCodeUtil::ConvertNonCallOpToProfiled(OpCode& op)
    {
        Assert(OpCodeAttr::HasProfiledOp(op));
        op += 1;
        Assert(OpCodeAttr::IsProfiledOp(op));
    }

    void OpCodeUtil::ConvertNonCallOpToProfiledWithICIndex(OpCode& op)
    {
        Assert(OpCodeAttr::HasProfiledOp(op) && OpCodeAttr::HasProfiledOpWithICIndex(op));
        op += 2;
        Assert(OpCodeAttr::IsProfiledOpWithICIndex(op));
    }

    void OpCodeUtil::ConvertNonCallOpToNonProfiled(OpCode& op)
    {
        if (OpCodeAttr::IsProfiledOp(op))
        {
            op -= 1;
            Assert(OpCodeAttr::HasProfiledOp(op));
        }
        else if (OpCodeAttr::IsProfiledOpWithICIndex(op))
        {
            op -= 2;
            Assert(OpCodeAttr::HasProfiledOpWithICIndex(op));
        }
        else
        {
            Assert(false);
        }
    }

    bool OpCodeUtil::IsCallOp(OpCode op)
    {
        return op >= Js::OpCode::CallI && op <= Js::OpCode::CallIExtendedFlags;
    }

    bool OpCodeUtil::IsProfiledCallOp(OpCode op)
    {
        return op >= Js::OpCode::ProfiledCallI && op <= Js::OpCode::ProfiledCallIExtendedFlags;
    }

    bool OpCodeUtil::IsProfiledCallOpWithICIndex(OpCode op)
    {
        return op >= Js::OpCode::ProfiledCallIWithICIndex && op <= Js::OpCode::ProfiledCallIExtendedFlagsWithICIndex;
    }

    bool OpCodeUtil::IsProfiledReturnTypeCallOp(OpCode op)
    {
        return op >= Js::OpCode::ProfiledReturnTypeCallI && op <= Js::OpCode::ProfiledReturnTypeCallIExtendedFlags;
    }

#if DBG
    OpCode OpCodeUtil::DebugConvertProfiledCallToNonProfiled(OpCode op)
    {
        switch (op)
        {
        case Js::OpCode::ProfiledCallI:
        case Js::OpCode::ProfiledCallIWithICIndex:
            return Js::OpCode::CallI;
        case Js::OpCode::ProfiledCallIFlags:
        case Js::OpCode::ProfiledCallIFlagsWithICIndex:
            return Js::OpCode::CallIFlags;
        case Js::OpCode::ProfiledCallIExtendedFlags:
        case Js::OpCode::ProfiledCallIExtendedFlagsWithICIndex:
            return Js::OpCode::CallIExtendedFlags;
        case Js::OpCode::ProfiledCallIExtended:
        case Js::OpCode::ProfiledCallIExtendedWithICIndex:
            return Js::OpCode::CallIExtended;
        default:
            Assert(false);
        };
        return Js::OpCode::Nop;
    }

    OpCode OpCodeUtil::DebugConvertProfiledReturnTypeCallToNonProfiled(OpCode op)
    {
        switch (op)
        {
        case Js::OpCode::ProfiledReturnTypeCallI:
            return Js::OpCode::CallI;
        case Js::OpCode::ProfiledReturnTypeCallIFlags:
            return Js::OpCode::CallIFlags;
        case Js::OpCode::ProfiledReturnTypeCallIExtendedFlags:
            return Js::OpCode::CallIExtendedFlags;
        case Js::OpCode::ProfiledReturnTypeCallIExtended:
            return Js::OpCode::CallIExtended;
        default:
            Assert(false);
        };

        return Js::OpCode::Nop;
    }
#endif

    OpCode OpCodeUtil::ConvertProfiledCallOpToNonProfiled(OpCode op)
    {
        OpCode newOpcode;
        if (IsProfiledCallOp(op))
        {
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledCallI + Js::OpCode::CallI);
        }
        else if (IsProfiledCallOpWithICIndex(op))
        {
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledCallIWithICIndex + Js::OpCode::CallI);
        }
        else
        {
            Assert(false);
            __assume(false);
        }
        Assert(DebugConvertProfiledCallToNonProfiled(op) == newOpcode);
        return newOpcode;
    }

    OpCode OpCodeUtil::ConvertProfiledReturnTypeCallOpToNonProfiled(OpCode op)
    {
        OpCode newOpcode;
        if (IsProfiledReturnTypeCallOp(op))
        {
            newOpcode = (OpCode)(op - Js::OpCode::ProfiledReturnTypeCallI + Js::OpCode::CallI);
        }
        else
        {
            Assert(false);
            __assume(false);
        }

        Assert(DebugConvertProfiledReturnTypeCallToNonProfiled(op) == newOpcode);
        return newOpcode;
    }

    OpCode OpCodeUtil::ConvertCallOpToProfiled(OpCode op, bool withICIndex)
    {
        return (!withICIndex) ?
            (OpCode)(op - OpCode::CallI + OpCode::ProfiledCallI) :
            (OpCode)(op - OpCode::CallI + OpCode::ProfiledCallIWithICIndex);
    }

    OpCode OpCodeUtil::ConvertCallOpToProfiledReturnType(OpCode op)
    {
        return (OpCode)(op - OpCode::CallI + OpCode::ProfiledReturnTypeCallI);
    }

    CompileAssert(((int)Js::OpCode::CallIExtendedFlags - (int)Js::OpCode::CallI) == ((int)Js::OpCode::ProfiledCallIExtendedFlags - (int)Js::OpCode::ProfiledCallI));
    CompileAssert(((int)Js::OpCode::CallIExtendedFlags - (int)Js::OpCode::CallI) == ((int)Js::OpCode::ProfiledReturnTypeCallIExtendedFlags - (int)Js::OpCode::ProfiledReturnTypeCallI));
    CompileAssert(((int)Js::OpCode::CallIExtendedFlags - (int)Js::OpCode::CallI) == ((int)Js::OpCode::ProfiledCallIExtendedFlagsWithICIndex - (int)Js::OpCode::ProfiledCallIWithICIndex));

    // Only include the opcode name on debug and test build
#if DBG_DUMP || ENABLE_DEBUG_CONFIG_OPTIONS

    wchar_t const * const OpCodeUtil::OpCodeNames[] =
    {
#define DEF_OP(x, y, ...) L"" STRINGIZEW(x) L"",
#include "OpCodeList.h"
#undef DEF_OP
    };

    wchar_t const * const OpCodeUtil::ExtendedOpCodeNames[] =
    {
#define DEF_OP(x, y, ...) L"" STRINGIZEW(x) L"",
#include "ExtendedOpCodeList.h"
#undef DEF_OP
    };

    wchar_t const * const OpCodeUtil::BackendOpCodeNames[] =
    {
#define DEF_OP(x, y, ...) L"" STRINGIZEW(x) L"",
#include "BackendOpCodeList.h"
#undef DEF_OP
    };

    wchar_t const * OpCodeUtil::GetOpCodeName(OpCode op)
    {
        if (op <= Js::OpCode::MaxByteSizedOpcodes)
        {
            Assert((uint)op < _countof(OpCodeNames));
            __analysis_assume((uint)op < _countof(OpCodeNames));
            return OpCodeNames[(uint)op];
        }
        else if (op < Js::OpCode::ByteCodeLast)
        {
            uint opIndex = op - (Js::OpCode::MaxByteSizedOpcodes + 1);
            Assert(opIndex < _countof(ExtendedOpCodeNames));
            __analysis_assume(opIndex < _countof(ExtendedOpCodeNames));
            return ExtendedOpCodeNames[opIndex];
        }
        uint opIndex = op - (Js::OpCode::ByteCodeLast + 1);
        Assert(opIndex < _countof(BackendOpCodeNames));
        __analysis_assume(opIndex < _countof(BackendOpCodeNames));
        return BackendOpCodeNames[opIndex];
    }

#else
    wchar const * OpCodeUtil::GetOpCodeName(OpCode op)
    {
        return L"<NotAvail>";
    }
#endif

    OpLayoutType const OpCodeUtil::OpCodeLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutType::y,
#include "OpCodeList.h"
    };

    OpLayoutType const OpCodeUtil::ExtendedOpCodeLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutType::y,
#include "ExtendedOpCodeList.h"
    };
    OpLayoutType const OpCodeUtil::BackendOpCodeLayouts[] =
    {
#define DEF_OP(x, y, ...) OpLayoutType::y,
#include "BackendOpCodeList.h"
    };

    OpLayoutType OpCodeUtil::GetOpCodeLayout(OpCode op)
    {
        if ((uint)op <= (uint)Js::OpCode::MaxByteSizedOpcodes)
        {
            AnalysisAssert((uint)op < _countof(OpCodeLayouts));
            return OpCodeLayouts[(uint)op];
        }
        else if (op < Js::OpCode::ByteCodeLast)
        {
            uint opIndex = op - (Js::OpCode::MaxByteSizedOpcodes + 1);
            AnalysisAssert(opIndex < _countof(ExtendedOpCodeLayouts));
            return ExtendedOpCodeLayouts[opIndex];
        }
        uint opIndex = op - (Js::OpCode::ByteCodeLast + 1);
        AnalysisAssert(opIndex < _countof(BackendOpCodeLayouts));
        return BackendOpCodeLayouts[opIndex];
    }

    bool OpCodeUtil::IsValidByteCodeOpcode(OpCode op)
    {
        CompileAssert((int)Js::OpCode::MaxByteSizedOpcodes + 1 + _countof(OpCodeUtil::ExtendedOpCodeLayouts) == (int)Js::OpCode::ByteCodeLast);
        return (uint)op < _countof(OpCodeLayouts)
            || (op > Js::OpCode::MaxByteSizedOpcodes && op < Js::OpCode::ByteCodeLast);
    }

    bool OpCodeUtil::IsValidOpcode(OpCode op)
    {
        return IsValidByteCodeOpcode(op)
            || (op > Js::OpCode::ByteCodeLast && op < Js::OpCode::Count);
    }
};
