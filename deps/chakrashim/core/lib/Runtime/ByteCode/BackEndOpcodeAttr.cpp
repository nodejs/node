//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"
#include "BackEndOpcodeAttr.h"

namespace OpCodeAttr
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
    None                        = 0x00000000,
    OpSideEffect                = 0x00000001, // If dst is unused and src can’t call implicitcalls, it still can not be dead-stored (Could throw an exception, etc)
    OpUseAllFields              = 0x00000002,

    OpTempNumberSources         = 0x00000004, // OpCode does support temp values as source
    OpTempNumberProducing       = 0x00000008, // OpCode can produce a temp value
    OpTempNumberTransfer        = 0x00000010 | OpTempNumberSources, // OpCode transfers a temp value

    OpTempObjectSources         = 0x00000020, // OpCode does support temp values as source
    OpTempObjectProducing       = 0x00000040, // OpCode can produce a temp value
    OpTempObjectTransfer        = 0x00000080 | OpTempObjectSources, // OpCode transfers a temp value
    OpTempObjectCanStoreTemp    = 0x00000100 | OpTempObjectProducing,  // OpCode can produce a temp value, and once marked, it will always produce a temp value so we can store other temp value in the object

    OpInlineCallInstr           = 0x00000200,
    OpCallsValueOf              = 0x00000400, // Could have valueOf/ToString side-effect
    OpCallInstr                 = 0x00000800,
    OpDoNotTransfer             = 0x00001000,
    OpHasImplicitCall           = 0x00002000, // Evaluating the src may call JS user code implicitly (getters/setters/valueof/tostring/DOM callbacks/etc)
    OpFastFldInstr              = 0x00004000,
    OpBailOutRec                = 0x00008000,

    OpInlinableBuiltIn          = 0x00010000, // OpCode is an inlineable built-in, such as InlineMathSin, etc.
    OpNonIntTransfer            = 0x00020000, // OpCode may transfer a non-integer value from the non-constant source to the destination
    OpIsInt32                   = 0x00040000, // OpCode converts its srcs to int32 or a narrower int type, and produces an int32
    OpProducesNumber            = 0x00080000, // OpCode always produces a number
    OpCanLoadFixedFields        = 0x00100000, // OpCode can use fixed fields
    OpCanCSE                    = 0x00200000, // Opcode has no side-effect and always produces the same value for a given input (InlineMathAbs is OK, InlineMathRandom is not)
    OpNoFallThrough             = 0x00400000, // Opcode doesn't fallthrough in flow and it always jumps to the return from this opcode
    OpPostOpDbgBailOut          = 0x00800000, // Generate bail out after this opcode. This must be a helper call and needs bailout on return from it. Used for Fast F12.

    OpHasMultiSizeLayout        = 0x01000000,
    OpHasProfiled               = 0x02000000,
    OpHasProfiledWithICIndex    = 0x04000000,
    OpDeadFallThrough           = 0x08000000,
    OpProfiled                  = 0x10000000, // OpCode is a profiled variant
    OpProfiledWithICIndex       = 0x20000000, // OpCode is a profiled with IC index variant
    OpBackEndOnly               = 0x40000000,
    OpByteCodeOnly              = 0x80000000,
};

static const int OpcodeAttributes[] =
{
#define DEF_OP(name, jnLayout, attrib, ...) attrib,
#include "ByteCode\OpCodeList.h"
#undef DEF_OP
};

static const int ExtendedOpcodeAttributes[] =
{
#define DEF_OP(name, jnLayout, attrib, ...) attrib,
#include "ByteCode\ExtendedOpCodeList.h"
#undef DEF_OP
};

static const int BackendOpCodeAttributes[] =
{
#define DEF_OP(name, jnLayout, attrib, ...) attrib,
#include "BackendOpCodeList.h"
#undef DEF_OP
};

static const int GetOpCodeAttributes(Js::OpCode op)
{
    if (op <= Js::OpCode::MaxByteSizedOpcodes)
    {
        AnalysisAssert(op < _countof(OpcodeAttributes));
        return OpcodeAttributes[(int)op];
    }
    else if (op < Js::OpCode::ByteCodeLast)
    {
        uint opIndex = op - (Js::OpCode::MaxByteSizedOpcodes + 1);
        AnalysisAssert(opIndex < _countof(ExtendedOpcodeAttributes));
        return ExtendedOpcodeAttributes[opIndex];
    }
    uint opIndex = op - (Js::OpCode::ByteCodeLast + 1);
    AnalysisAssert(opIndex < _countof(BackendOpCodeAttributes));
    return BackendOpCodeAttributes[opIndex];
}

bool HasSideEffects(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpSideEffect) != 0);
};
bool CanCSE(Js::OpCode opcode)
{
    Assert(((GetOpCodeAttributes(opcode) & OpCanCSE) == 0) || ((GetOpCodeAttributes(opcode) & OpSideEffect) == 0));
    return ((GetOpCodeAttributes(opcode) & OpCanCSE) != 0);
};
bool UseAllFields(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpUseAllFields) != 0);
}
bool NonTempNumberSources(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempNumberSources) == 0);
}
bool TempNumberSources(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempNumberSources) != 0);
}
bool TempNumberProducing(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempNumberProducing) != 0);
}
bool TempNumberTransfer(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempNumberTransfer) == OpTempNumberTransfer);
}

bool TempObjectSources(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempObjectSources) != 0);
}
bool TempObjectProducing(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempObjectProducing) != 0);
}
bool TempObjectTransfer(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempObjectTransfer) == OpTempObjectTransfer);
}
bool TempObjectCanStoreTemp(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpTempObjectCanStoreTemp) == OpTempObjectCanStoreTemp);
}
bool CallInstr(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpCallInstr) != 0);
}
bool InlineCallInstr(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpInlineCallInstr) != 0);
}
bool CallsValueOf(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpCallsValueOf) != 0);
}
bool FastFldInstr(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpFastFldInstr) != 0);
}
bool BailOutRec(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpBailOutRec) != 0);
}
bool ByteCodeOnly(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpByteCodeOnly) != 0);
}
bool BackEndOnly(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpBackEndOnly) != 0);
}
bool DoNotTransfer(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpDoNotTransfer) != 0);
}
bool HasImplicitCall(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpHasImplicitCall) != 0);
}
bool IsProfiledOp(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpProfiled) != 0);
}
bool IsProfiledOpWithICIndex(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpProfiledWithICIndex) != 0);
}
bool IsInlineBuiltIn(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpInlinableBuiltIn) != 0);
}
bool NonIntTransfer(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpNonIntTransfer) != 0);
}
bool IsInt32(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpIsInt32) != 0);
}
bool ProducesNumber(Js::OpCode opcode, Js::ScriptContext *const scriptContext)
{
    return ((GetOpCodeAttributes(opcode) & OpProducesNumber) != 0);
}
bool HasFallThrough(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpNoFallThrough) == 0);
}
bool NeedsPostOpDbgBailOut(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpPostOpDbgBailOut) != 0);
}

bool HasMultiSizeLayout(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpHasMultiSizeLayout) != 0);
}

bool HasProfiledOp(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpHasProfiled) != 0);
}
bool HasProfiledOpWithICIndex(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpHasProfiledWithICIndex) != 0);
}
bool HasDeadFallThrough(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpDeadFallThrough) != 0);
}
bool CanLoadFixedFields(Js::OpCode opcode)
{
    return ((GetOpCodeAttributes(opcode) & OpCanLoadFixedFields) != 0);
}

}; // OpCodeAttr
