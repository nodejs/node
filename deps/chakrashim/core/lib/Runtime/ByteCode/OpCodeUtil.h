//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
class OpCodeUtil
{
public:
    static wchar_t const * GetOpCodeName(OpCode op);

    static bool IsCallOp(OpCode op);
    static bool IsProfiledCallOp(OpCode op);
    static bool IsProfiledCallOpWithICIndex(OpCode op);
    static bool IsProfiledReturnTypeCallOp(OpCode op);

    // OpCode conversion functions
    static void ConvertOpToNonProfiled(OpCode& op);
    static void ConvertNonCallOpToProfiled(OpCode& op);
    static void ConvertNonCallOpToProfiledWithICIndex(OpCode& op);
    static void ConvertNonCallOpToNonProfiled(OpCode& op);
    static void ConvertNonCallOpToNonProfiledWithICIndex(OpCode& op);

    static OpCode ConvertProfiledCallOpToNonProfiled(OpCode op);
    static OpCode ConvertProfiledReturnTypeCallOpToNonProfiled(OpCode op);
    static OpCode ConvertCallOpToProfiled(OpCode op, bool withICIndex = false);
    static OpCode ConvertCallOpToProfiledReturnType(OpCode op);

    static bool IsValidByteCodeOpcode(OpCode op);
    static bool IsValidOpcode(OpCode op);
    static bool IsPrefixOpcode(OpCode op);
    static bool IsSmallEncodedOpcode(OpCode op);
    static uint EncodedSize(OpCode op, LayoutSize layoutSize);

    static OpLayoutType GetOpCodeLayout(OpCode op);
private:
#if DBG_DUMP || ENABLE_DEBUG_CONFIG_OPTIONS
    static wchar_t const * const OpCodeNames[(int)Js::OpCode::MaxByteSizedOpcodes + 1];
    static wchar_t const * const ExtendedOpCodeNames[];
    static wchar_t const * const BackendOpCodeNames[];
#endif
    static OpLayoutType const OpCodeLayouts[];
    static OpLayoutType const ExtendedOpCodeLayouts[];
    static OpLayoutType const BackendOpCodeLayouts[];
#if DBG
    static OpCode DebugConvertProfiledCallToNonProfiled(OpCode op);
    static OpCode DebugConvertProfiledReturnTypeCallToNonProfiled(OpCode op);
#endif
};
};
