//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

namespace OpCodeAttrAsmJs
{
    // OpSideEffect:
    //      Opcode has side effect not just to the dst/src on the instruction.
    //      The opcode cannot be deadstored. (e.g. StFld, LdFld from DOM, call valueOf/toString/getter/setter)
    //      Doesn't include all "exit" script (e.g. LdThis doesn't have side effect for HostDispatch for exiting script to getting the name space parent)
    // OpHasImplicitCall:
    //      Include all possible exit scripts, call valueOf/toString/getter/setter
    // OpSerialized:
    //      Op is a serialized (indirected) variant of another op code
    enum OpCodeAttrEnum
    {
        None = 0,
        OpNoFallThrough = 1 << 0, // Opcode doesn't fallthrough in flow  and its always jump to the return from this opcode.
        OpHasMultiSizeLayout = 1 << 1,

    };

    static const int OpcodeAttributesAsmJs[] =
    {
#define DEF_OP(name, jnLayout, attrib, ...) attrib,
#include "ByteCode\OpCodeListAsmJs.h"
#undef DEF_OP
    };

    static const int ExtendedOpcodeAttributesAsmJs[] =
    {
#define DEF_OP(name, jnLayout, attrib, ...) attrib,
#include "ByteCode\ExtendedOpCodeListAsmJs.h"
#undef DEF_OP
    };


    static const int GetOpCodeAttributes( Js::OpCodeAsmJs op )
    {
        uint opIndex = (uint)op;
        if (opIndex <= (uint)Js::OpCodeAsmJs::MaxByteSizedOpcodes)
        {
            AnalysisAssert(opIndex < _countof(OpcodeAttributesAsmJs));
            return OpcodeAttributesAsmJs[opIndex];
        }
        opIndex -= ( Js::OpCodeAsmJs::MaxByteSizedOpcodes + 1 );
        AnalysisAssert(opIndex < _countof(ExtendedOpcodeAttributesAsmJs));
        return ExtendedOpcodeAttributesAsmJs[opIndex];
    }

#define CheckHasFlag(flag) (!!(GetOpCodeAttributes(opcode) & flag))
#define CheckNoHasFlag(flag) (!(GetOpCodeAttributes(opcode) & flag))


    bool HasFallThrough( Js::OpCodeAsmJs opcode )
    {
        return CheckNoHasFlag( OpNoFallThrough );
    }

    bool HasMultiSizeLayout( Js::OpCodeAsmJs opcode )
    {
        return CheckHasFlag( OpHasMultiSizeLayout );
    }


}; // OpCodeAttrAsmJs
