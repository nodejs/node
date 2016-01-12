//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace OpCodeAttr
{
    // True if the opcode has side effect other then assigning to the dst
    bool HasSideEffects(Js::OpCode opcode);
    // True if the opcode has not side-effect and always produces the same value for a given input
    bool CanCSE(Js::OpCode opcode);
    // True if the opcode implicitly (may) use any the fields
    bool UseAllFields(Js::OpCode opcode);
    // True if the opcode don't allow temp numbers sources (e.g. StFld, Ret, StElem)
    bool NonTempNumberSources(Js::OpCode opcode);
    // !NonTempSources
    bool TempNumberSources(Js::OpCode opcode);
    // True if the opcode can use a temporary stack slot for it's number result
    bool TempNumberProducing(Js::OpCode opcode);
    // True if the source number may assign directly to the dest number
    bool TempNumberTransfer(Js::OpCode opcode);
    // True if the opcode allows temp object sources
    bool TempObjectSources(Js::OpCode opcode);
    // True if the opcode can generate object on the stack
    bool TempObjectProducing(Js::OpCode opcode);
    // True if the opcode can generate object on the stack and will once marked so other temp object/number can be stored
    bool TempObjectCanStoreTemp(Js::OpCode opcode);
    // True if the source object may assign directly to the dest object
    bool TempObjectTransfer(Js::OpCode opcode);
    // True for call instructions
    bool CallInstr(Js::OpCode opcode);
    // True for call instructions which may get inlined
    bool InlineCallInstr(Js::OpCode opcode);
    // True if the opcode may have the side-effect of calling valueof().
    bool CallsValueOf(Js::OpCode opcode);
    // True if the opcode can do optimizations on property syms.
    bool FastFldInstr(Js::OpCode opcode);
    // True if the opcode requires a bailout record.
    bool BailOutRec(Js::OpCode opcode);
    // True if the opcode can only appear in the byte code
    bool ByteCodeOnly(Js::OpCode opcode);
    // True if the opcode can only appear in the back end
    bool BackEndOnly(Js::OpCode opcode);
    // True if the opcode does not transfer value from src to dst (only some opcode are marked property)
    bool DoNotTransfer(Js::OpCode opcode);
    // True if the opcode may have implicit call
    bool HasImplicitCall(Js::OpCode opcode);
    // True if the opcode is a profiled variant
    bool IsProfiledOp(Js::OpCode opcode);
    // True if the opcode is a profiled variant with an inline cache index
    bool IsProfiledOpWithICIndex(Js::OpCode opcode);
    // True if the opcode is a math helper, such as sin, cos, pow,..
    bool IsInlineBuiltIn(Js::OpCode opcode);
    // True if the opcode may transfer a non-integer value from the non-constant source to the destination
    bool NonIntTransfer(Js::OpCode opcode);
    // True if the opcode converts its srcs to int32 or a narrower int type, and produces an int32
    bool IsInt32(Js::OpCode opcode);
    // True if the opcode always produces a number in the specified script context's version
    bool ProducesNumber(Js::OpCode opcode, Js::ScriptContext *const scriptContext);
    // False if the opcode results in jump to end of the function and there cannot be fallthrough.
    bool HasFallThrough(Js::OpCode opcode);
    // True if we need to generate bailout after this opcode when in debug mode (b/o on return from helper).
    bool NeedsPostOpDbgBailOut(Js::OpCode opcode);
    // True if the opcode has a small/large layout
    bool HasMultiSizeLayout(Js::OpCode opcode);
    // True if the opcode has a profiled version of the opcode
    bool HasProfiledOp(Js::OpCode opcode);
    // True if the opcode has a profiled version of the opcode with an inline cache index
    bool HasProfiledOpWithICIndex(Js::OpCode opcode);
    // True if the opcode will never fallthrough (e.g. guaranteed bailout)
    bool HasDeadFallThrough(Js::OpCode opcode);
    // True if the opcode can use fixed fields
    bool CanLoadFixedFields(Js::OpCode opcode);
};
