//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Default all macro to nothing
#ifndef MACRO
#define MACRO( opcode, layout, attr)
#endif

#ifndef MACRO_WMS
#define MACRO_WMS(opcode, layout, attr)
#endif

#ifndef MACRO_EXTEND
#define MACRO_EXTEND(opcode, layout, attr)
#endif

#ifndef MACRO_EXTEND_WMS
#define MACRO_EXTEND_WMS(opcode, layout, attr)
#endif

#ifndef MACRO_BACKEND_ONLY
#define MACRO_BACKEND_ONLY(opcode, layout, attr)
#endif

#define MACRO_WMS_PROFILED( opcode, layout, attr) \
    MACRO_WMS(opcode, layout, OpHasProfiled|attr) \
    MACRO_WMS(Profiled##opcode, Profiled##layout, OpByteCodeOnly|OpProfiled|attr) \

#define MACRO_WMS_PROFILED2(opcode, layout, attr) \
    MACRO_WMS(opcode, layout, OpHasProfiled|attr) \
    MACRO_WMS(Profiled##opcode, Profiled2##layout, OpByteCodeOnly|OpProfiled|attr) \

#define MACRO_WMS_PROFILED_INDEX(opcode, layout, attr) \
    MACRO_WMS(opcode, layout, OpHasProfiled|OpHasProfiledWithICIndex|attr) \
    MACRO_WMS(Profiled##opcode, Profiled##layout, OpByteCodeOnly|OpProfiled|attr) \
    MACRO_WMS(Profiled##opcode##WithICIndex, Profiled##layout##WithICIndex, OpByteCodeOnly|OpProfiledWithICIndex|attr) \

#define MACRO_WMS_PROFILED_OP(  opcode, layout, attr) \
    MACRO_WMS(opcode, layout, OpHasProfiled|attr) \
    MACRO_WMS(Profiled##opcode, layout, OpByteCodeOnly|OpProfiled|attr) \

#define MACRO_EXTEND_WMS_AND_PROFILED_OP(opcode, layout, attr) \
    MACRO_EXTEND_WMS(opcode, layout, OpHasProfiled | attr) \
    MACRO_EXTEND_WMS(Profiled##opcode, layout, OpByteCodeOnly | OpProfiled | attr) \


#define MACRO_PROFILED(opcode, layout, attr) \
    MACRO(opcode, layout, OpHasProfiled|attr) \
    MACRO(Profiled##opcode, Profiled##layout, OpByteCodeOnly|OpProfiled|attr) \

#define MACRO_EXTEND_WMS_AND_PROFILED(opcode, layout, attr) \
    MACRO_EXTEND_WMS(opcode, layout, OpHasProfiled|attr) \
    MACRO_EXTEND_WMS(Profiled##opcode, Profiled##layout, OpByteCodeOnly|OpProfiled|attr) \

MACRO(                  EndOfBlock,         Empty,          OpByteCodeOnly)     // End-of-buffer
MACRO(                  ExtendedOpcodePrefix,Empty,         OpByteCodeOnly)
MACRO(                  MediumLayoutPrefix,  Empty,         OpByteCodeOnly)
MACRO(                  ExtendedMediumLayoutPrefix,Empty,   OpByteCodeOnly)
MACRO(                  LargeLayoutPrefix,  Empty,          OpByteCodeOnly)
MACRO(                  ExtendedLargeLayoutPrefix,Empty,    OpByteCodeOnly)

MACRO(                  Nop,                        Empty,          None)       // No operation (Default value = 0)
MACRO(                  StartCall,          StartCall,      OpSideEffect)
MACRO_BACKEND_ONLY(     LoweredStartCall,   StartCall,      OpSideEffect)       // StartCall instruction after it's been lowered
MACRO_BACKEND_ONLY(     StartCallAsmJsI,    StartCall,      OpSideEffect)       // StartCall instruction for asm.js internal calls
MACRO_BACKEND_ONLY(     StartCallAsmJsE,    StartCall,      OpSideEffect)       // StartCall instruction for calls from asm.js to javascript
MACRO(                  Break,              Empty,          OpSideEffect)       // Break into debugger
MACRO_EXTEND(           InvalidOpCode,      Empty,          None)               // Inserted in a dead call sequence, should not be present after GlobOpt

// Control flow
#ifdef BYTECODE_BRANCH_ISLAND
MACRO_EXTEND(           BrLong,             BrLong,         OpByteCodeOnly|OpSideEffect|OpNoFallThrough)
#endif
MACRO(                  Br ,                Br,             OpSideEffect|OpNoFallThrough)           // Unconditional branch
MACRO_BACKEND_ONLY(     MultiBr,            Empty,          OpSideEffect|OpNoFallThrough)           // Unconditional multi-target branch

MACRO_BACKEND_ONLY(     BrHasSideEffects,   BrS,            OpSideEffect|OpTempNumberSources)       // Branch if there are any of the side effects in val
MACRO(                  BrNotHasSideEffects,BrS,            OpSideEffect|OpTempNumberSources)       // Branch if there are none of the side effects in val

MACRO_BACKEND_ONLY(     BrFncEqApply ,      BrReg1,         OpSideEffect|OpTempNumberSources)       // Branch if R1 is func whose native code ptr == Apply
MACRO_WMS(              BrFncNeqApply ,     BrReg1,         OpSideEffect|OpTempNumberSources)       // Branch if R1 is func whose native code ptr != Apply

MACRO_WMS(              BrEq_A,             BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if '=='  (general equals)
MACRO_WMS(              BrNeq_A,            BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if '!='  (not general equals)
MACRO_WMS(              BrFalse_A,          BrReg1,         OpSideEffect|OpTempNumberSources|OpTempObjectSources)                          // Branch if 'false' (NOTE: do not call ToPrimitive, no implicit call)
MACRO_WMS(              BrTrue_A,           BrReg1,         OpSideEffect|OpTempNumberSources|OpTempObjectSources)                          // Branch if 'true' (NOTE: do not call ToPrimitive, no implicit call)
MACRO_WMS(              BrGe_A,             BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '>='
MACRO_BACKEND_ONLY(     BrUnGe_A,           BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '>='
MACRO_WMS(              BrGt_A,             BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '>'
MACRO_BACKEND_ONLY(     BrUnGt_A,           BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '>'
MACRO_WMS(              BrLt_A,             BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '<'
MACRO_BACKEND_ONLY(     BrUnLt_A,           BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '<'
MACRO_WMS(              BrLe_A,             BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '<='
MACRO_BACKEND_ONLY(     BrUnLe_A,           BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)        // Branch if '<='
MACRO_WMS(              BrSrEq_A,           BrReg2,         OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if '==='  (strict equals)       (NOTE: May have DOM implicit calls)
MACRO_WMS(              BrSrNeq_A,          BrReg2,         OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if '!=='  (not strict equals)   (NOTE: May have DOM implicit calls)
MACRO_EXTEND(           BrOnHasProperty,    BrProperty,     OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if object has the given property (NOTE: May have DOM implicit calls)
MACRO(                  BrOnNoProperty,     BrProperty,     OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if object does not have the given property (NOTE: May have DOM implicit calls)
MACRO(                  BrOnNoLocalProperty,BrLocalProperty,OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if object does not have the given property (NOTE: May have DOM implicit calls)
MACRO(                  BrOnNoEnvProperty,  BrEnvProperty,  OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources)        // Branch if object does not have the given property (NOTE: May have DOM implicit calls)
MACRO_WMS(              BrOnObject_A,       BrReg1,         OpSideEffect|OpTempNumberSources|OpTempObjectSources)                          // Branch if typeId is not primitive type (i.e. > TypeIds_LastJavascriptPrimitiveType)
MACRO_WMS(              BrNotNull_A,        BrReg1,         OpSideEffect|OpTempNumberSources|OpTempObjectSources)                          // Branch if not NULL
MACRO_EXTEND_WMS(       BrNotUndecl_A,      BrReg1,         OpSideEffect|OpTempNumberSources|OpTempObjectSources)                          // Branch if source reg is NEQ to Undecl
MACRO_BACKEND_ONLY(     BrNotEq_A,          Empty,          OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Branch if !'>='
MACRO_BACKEND_ONLY(     BrNotNeq_A,         Empty,          OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Branch if !'>='
MACRO_BACKEND_ONLY(     BrSrNotEq_A,        Empty,          OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Branch if !'>='
MACRO_BACKEND_ONLY(     BrSrNotNeq_A,       Empty,          OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Branch if !'>='
MACRO_BACKEND_ONLY(     BrNotGe_A,          Empty,          OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut) // Branch if !'>='
MACRO_BACKEND_ONLY(     BrNotGt_A,          Empty,          OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut) // Branch if !'>'
MACRO_BACKEND_ONLY(     BrNotLt_A,          Empty,          OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut) // Branch if !'<'
MACRO_BACKEND_ONLY(     BrNotLe_A,          Empty,          OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut) // Branch if !'<='
MACRO_BACKEND_ONLY(     BrNotAddr_A,        BrReg2,         OpSideEffect|OpTempNumberSources|OpTempObjectSources)           // Branch if != Address
MACRO_BACKEND_ONLY(     BrAddr_A,           BrReg2,         OpSideEffect|OpTempNumberSources|OpTempObjectSources)           // Branch if == Address
MACRO_BACKEND_ONLY(     BrOnException,      Br,             OpSideEffect)
MACRO_BACKEND_ONLY(     BrOnNoException,    Br,             OpSideEffect) // Mark it OpNoFallThrough?

MACRO_BACKEND_ONLY(     DeadBrEqual,        Reg3,           OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Dead BrEqual, but still need to reference srcs
MACRO_BACKEND_ONLY(     DeadBrSrEqual,      Reg3,           OpSideEffect|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Dead BrSrEqual, but still need to reference srcs
MACRO_BACKEND_ONLY(     DeadBrRelational,   Reg3,           OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Dead BrRelational, but still need to reference srcs
MACRO_BACKEND_ONLY(     DeadBrOnHasProperty,Reg2,           OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Dead BrOnHasProperty, but still need to reference srcs
MACRO_BACKEND_ONLY(     DeletedNonHelperBranch,Empty,       OpSideEffect)        // Deleted branch to non-helper.  Used to avoid opHleper asserts.

MACRO_BACKEND_ONLY(     BrEq_I4,            Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if '=='  (general equals)
MACRO_BACKEND_ONLY(     BrTrue_I4,          Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if 'true'
MACRO_BACKEND_ONLY(     BrFalse_I4,         Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if 'false'
MACRO_BACKEND_ONLY(     BrGe_I4,            Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if '>='
MACRO_BACKEND_ONLY(     BrUnGe_I4,          Empty,          OpSideEffect|OpTempNumberSources)        // uint32 Branch if '>='
MACRO_BACKEND_ONLY(     BrGt_I4,            Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if '>'
MACRO_BACKEND_ONLY(     BrUnGt_I4,          Empty,          OpSideEffect|OpTempNumberSources)        // uint32 Branch if '>'
MACRO_BACKEND_ONLY(     BrLt_I4,            Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if '<'
MACRO_BACKEND_ONLY(     BrUnLt_I4,          Empty,          OpSideEffect|OpTempNumberSources)        // uint32 Branch if '<'
MACRO_BACKEND_ONLY(     BrLe_I4,            Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if '<='
MACRO_BACKEND_ONLY(     BrUnLe_I4,          Empty,          OpSideEffect|OpTempNumberSources)        // uint32 Branch if '<='
MACRO_BACKEND_ONLY(     BrNeq_I4,           Empty,          OpSideEffect|OpTempNumberSources)        // int32 Branch if '!='  (not general equals)

// For Switch Case optimization
MACRO(                  EndSwitch,          Br,             OpSideEffect)                       // Unconditional branch - This is equivalent to Br OpCode
MACRO_WMS(              Case,               BrReg2,         OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources) // Equivalent to Branch if '===' (strict equals) (NOTE: May have DOM implicit calls)
MACRO_WMS_PROFILED(     BeginSwitch,        Reg2,           OpTempNumberTransfer|OpTempObjectTransfer|OpNonIntTransfer)          // Switch statement - identifies the start of a switch statement
                                                                                                                                 // performs the function of Ld_A and does profiling

// All the Call* function need to be together so that ProfileCall* can be calculated
MACRO_BACKEND_ONLY(     Call,               Reg1,           OpSideEffect|OpUseAllFields|OpCallInstr)        // R0 <- Call (direct) registered function
MACRO_BACKEND_ONLY(     AsmJsCallI,         Reg1,           OpSideEffect|OpUseAllFields|OpCallInstr)        // call from asm.js to asm.js
MACRO_BACKEND_ONLY(     AsmJsCallE,         Reg1,           OpSideEffect|OpUseAllFields|OpCallInstr)        // call from asm.js to javascript

// CallI through CallIExtendedFlags need to stay in this order since all the ProfiledCall* opcodes are calculated based on this order
MACRO_WMS(              CallI,              CallI,          OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)          // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              CallIFlags,         CallIFlags,     OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)          // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              CallIExtended,      CallIExtended,  OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_WMS(              CallIExtendedFlags, CallIExtendedFlags, OpSideEffect|OpUseAllFields|OpCallInstr)

MACRO_BACKEND_ONLY(     CallIPut,           CallIFlags,     OpSideEffect|OpUseAllFields|OpCallInstr)        // Call (indirect) Function(ArgCount) to put value
MACRO_BACKEND_ONLY(     CallINew,           CallIFlags,     OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_BACKEND_ONLY(     CallINewTargetNew,  CallIFlags,     OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_BACKEND_ONLY(     CallIExtendedNew,   CallIExtendedFlags, OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_BACKEND_ONLY(     CallIEval,          CallIExtendedFlags, OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_BACKEND_ONLY(     CallIExtendedNewTargetNew, CallIExtendedFlags, OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_BACKEND_ONLY(     CallIDynamic,       CallI,          OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_BACKEND_ONLY(     CallIDynamicSpread, CallI,          OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_BACKEND_ONLY(     CallDirect,         Empty,          OpTempNumberSources|OpCallInstr|OpSideEffect|OpHasImplicitCall|OpTempObjectProducing)     // For direct calls to helper (used in inlining built-ins)

MACRO_BACKEND_ONLY(     InlineThrow,        Reg1,           OpSideEffect|OpPostOpDbgBailOut|OpDeadFallThrough)  // Throw exception
MACRO_BACKEND_ONLY(     EHThrow,            Reg1,           OpSideEffect|OpPostOpDbgBailOut|OpDeadFallThrough)  // Throw exception
MACRO_WMS(              Throw,              Reg1,           OpSideEffect|OpNoFallThrough|OpPostOpDbgBailOut)    // Throw exception
MACRO(                  Ret,                Empty,          OpSideEffect|OpUseAllFields|OpNoFallThrough)        // Return from function
MACRO_WMS(              Yield,              Reg2,           OpSideEffect|OpUseAllFields)                        // Yield from generator function
MACRO_WMS(              ResumeYield,        Reg2,           OpSideEffect)
MACRO_WMS(              ResumeYieldStar,    Reg3,           OpSideEffect)
MACRO_EXTEND_WMS(       AsyncSpawn,         Reg3,           OpSideEffect|OpUseAllFields)

// Unary operations
MACRO_WMS(              Incr_A,             Reg2,           OpTempNumberProducing|OpCallsValueOf|OpDoNotTransfer|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)     // Increment
MACRO_WMS(              Decr_A,             Reg2,           OpTempNumberProducing|OpCallsValueOf|OpDoNotTransfer|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)     // Decrement
MACRO_WMS(              Neg_A,              Reg2,           OpTempNumberProducing|OpCallsValueOf|OpDoNotTransfer|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)     // Arithmetic '-' (negate)
MACRO_WMS(              Not_A,              Reg2,           OpTempNumberProducing|OpCallsValueOf|OpDoNotTransfer|OpIsInt32|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber) // Boolean '!' (not)

// Integer Unary operations
MACRO_BACKEND_ONLY(     Neg_I4,             Empty,          OpDoNotTransfer|OpTempNumberSources|OpCanCSE)       // int32 Arithmetic '-' (negate)
MACRO_BACKEND_ONLY(     Not_I4,             Empty,          OpDoNotTransfer|OpTempNumberSources|OpCanCSE)       // int32 '!' (not)

// NOTE:
// Typeof
//      Only have external call for HostDispatch, but only QI, and shouldn't have direct side effect,
//      so we can dead store it but still need to check for implicit call.
//      It also doesn't call valueOf/toString
//      TypeofElem
//      May throw exception if x (in x.y or x[y]) is null or undefined, so it can't be dead store (OpSideEffect)
//      GetProperty may call getter, Typeof may have external call for HostDispatch
//      It also doesn't call valueOf/toString
//      REVIEW: TypeofRootFld will be off the root object which can't be null?  So we could remove OpSideEffect?

MACRO_WMS(              Typeof,             Reg2,           OpTempNumberProducing|OpDoNotTransfer|OpHasImplicitCall|OpCanCSE)                         // typeof x
MACRO_WMS(              TypeofElem,         ElementI,       OpTempNumberProducing|OpSideEffect|OpDoNotTransfer|OpHasImplicitCall|OpPostOpDbgBailOut)  // typeof x[y]

MACRO_WMS(              ApplyArgs,          Reg5,           OpByteCodeOnly|OpSideEffect|OpUseAllFields)     // apply function to this function's arguments and this pointer

// Binary operations
MACRO_WMS(              Add_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)  // Arithmetic '+'
MACRO_WMS_PROFILED(     Div_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)  // Arithmetic '/'
MACRO_WMS(              Mul_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)  // Arithmetic '*'
MACRO_WMS_PROFILED(     Rem_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)  // Arithmetic '%'
MACRO_WMS(              Sub_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)  // Arithmetic '-' (subtract)
MACRO_WMS(              Expo_A,             Reg3,           OpTempNumberProducing|OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)  // Arithmetic '**' (exponentiation)

MACRO_WMS(              And_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpIsInt32|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber) // Bitwise '&'
MACRO_WMS(              Or_A,               Reg3,           OpTempNumberProducing|OpCallsValueOf|OpIsInt32|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber) // Bitwise '|'
MACRO_WMS(              Xor_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpIsInt32|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber) // Bitwise '^'
MACRO_WMS(              Shl_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpIsInt32|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber) // Shift '<<' (signed, truncate)
MACRO_WMS(              Shr_A,              Reg3,           OpTempNumberProducing|OpCallsValueOf|OpIsInt32|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpProducesNumber) // Shift '>>' (signed, truncate)
MACRO_WMS(              ShrU_A,             Reg3,           OpTempNumberProducing|OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut|OpProducesNumber)           // Shift '>>>'(unsigned, truncate)

// Integer Binary operations used only by backend, hence no need for one byte variant
MACRO_BACKEND_ONLY(     Add_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Arithmetic '+'
MACRO_BACKEND_ONLY(     Sub_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Arithmetic '-' (subtract)
MACRO_BACKEND_ONLY(     Mul_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Arithmetic '*'
MACRO_BACKEND_ONLY(     Div_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Arithmetic '/'
MACRO_BACKEND_ONLY(     Rem_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Arithmetic '%'
MACRO_BACKEND_ONLY(     And_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Bitwise '&'
MACRO_BACKEND_ONLY(     Or_I4,              Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Bitwise '|'
MACRO_BACKEND_ONLY(     Xor_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Bitwise '^'
MACRO_BACKEND_ONLY(     Shl_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Shift '<<' (signed, truncate)
MACRO_BACKEND_ONLY(     Shr_I4,             Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Shift '>>' (signed, truncate)
MACRO_BACKEND_ONLY(     ShrU_I4,            Empty,          OpTempNumberSources|OpCanCSE)                                    // int32 Shift '>>>'(unsigned, truncate)

MACRO_BACKEND_ONLY(     Add_Ptr,            Empty,          OpTempNumberSources|OpCanCSE)                                    // ptr Arithmetic '+'

// Comparison
MACRO_WMS(              CmEq_A,             Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE)                          // Compare if '=='  (general equals)
MACRO_WMS(              CmGe_A,             Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '>='
MACRO_BACKEND_ONLY(     CmUnGe_A,           Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '>='
MACRO_WMS(              CmGt_A,             Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '>'
MACRO_BACKEND_ONLY(     CmUnGt_A,           Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '>'
MACRO_WMS(              CmLt_A,             Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '<'
MACRO_BACKEND_ONLY(     CmUnLt_A,           Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '<'
MACRO_WMS(              CmLe_A,             Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '<='
MACRO_BACKEND_ONLY(     CmUnLe_A,           Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)       // Compare if '<='
MACRO_WMS(              CmNeq_A,            Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE)                          // Compare if '!='  (not general equals)
MACRO_WMS(              CmSrEq_A,           Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpTempObjectSources)      // Compare if '==='  (strict equals)     (NOTE: May have DOM implicit calls)
MACRO_WMS(              CmSrNeq_A,          Reg3,           OpCallsValueOf|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpTempObjectSources)      // Compare if '!=='  (not strict equals) (NOTE: May have DOM implicit calls)

MACRO_BACKEND_ONLY(     CmEq_I4,            Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '=='
MACRO_BACKEND_ONLY(     CmNeq_I4,           Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '!='
MACRO_BACKEND_ONLY(     CmLt_I4,            Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '<'
MACRO_BACKEND_ONLY(     CmLe_I4,            Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '<='
MACRO_BACKEND_ONLY(     CmGt_I4,            Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '>'
MACRO_BACKEND_ONLY(     CmGe_I4,            Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '>='
MACRO_BACKEND_ONLY(     CmUnLt_I4,          Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '<'
MACRO_BACKEND_ONLY(     CmUnLe_I4,          Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '<='
MACRO_BACKEND_ONLY(     CmUnGt_I4,          Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '>'
MACRO_BACKEND_ONLY(     CmUnGe_I4,          Reg3,           OpTempNumberSources|OpCanCSE)                                   // Unsigned I4 Compare if '>='


// Conversions
MACRO_WMS(              Conv_Num,           Reg2,           OpSideEffect|OpTempNumberProducing|OpTempNumberTransfer|OpTempObjectSources|OpCallsValueOf|OpProducesNumber) // Convert to Number. [[ToNumber()]]
// Operation ToString(str)
MACRO_EXTEND_WMS(       Conv_Str,           Reg2,           OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)

// Conv_Obj:
//      OpSideEffect - May throw exception on null/undefined.
//      Do not call valueOf/toString no implicit call
MACRO_WMS(              Conv_Obj,           Reg2,           OpSideEffect|OpPostOpDbgBailOut|OpTempObjectTransfer)   // Convert to Object
MACRO_EXTEND_WMS(       NewWithObject,      Reg2,           OpSideEffect | OpPostOpDbgBailOut)  // Wrap in a with Object
MACRO_BACKEND_ONLY(     ToVar,              Reg2,           OpTempNumberProducing|OpTempNumberSources|OpCanCSE)     // Load from int32/float64 to Var(reg)
// Load from Var(reg) to int32/float64, NOTE: always bail if it is not primitive. so no implicit call, but still mark with CallsValueOf so it won't get automatically dead stored
// TODO: Consider changing the code so we don't have mark this as CallsValueOf
MACRO_BACKEND_ONLY(     FromVar,            Reg2,           OpTempNumberSources|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     Conv_Prim,          Reg2,           OpTempNumberProducing|OpTempNumberSources|OpCanCSE|OpPostOpDbgBailOut)  // Convert between primitives (int32/float64)
MACRO_BACKEND_ONLY(     Conv_Bool,          Reg2,           OpTempNumberSources|OpCanCSE)                           // Convert from i4 to bool


// Register
MACRO_EXTEND_WMS(       UnwrapWithObj,      Reg2,           OpSideEffect) // Copy Var register with unwrapped object
MACRO_EXTEND_WMS(       SetComputedNameVar, Reg2,           OpSideEffect)
MACRO_WMS(              Ld_A,               Reg2,           OpTempNumberTransfer|OpTempObjectTransfer|OpNonIntTransfer|OpCanCSE) // Copy Var register
MACRO_WMS(              LdLocalObj,         Reg1,           OpCanCSE) // Load non-stack frame object
MACRO_WMS(              LdInnerScope,       Reg1Unsigned1,  OpCanCSE) // Load non-stack inner scope
MACRO_WMS(              LdC_A_Null,         Reg1,           OpByteCodeOnly|OpCanCSE)   // Load from 'null' as Var
MACRO_BACKEND_ONLY(     Ld_I4,              Empty,          OpCanCSE)                  // Copy I4 register
MACRO_BACKEND_ONLY(     LdC_A_I4,           Empty,          OpCanCSE)                  // Load from 'int32' as Var(C)
MACRO_BACKEND_ONLY(     LdC_A_R8,           Empty,          OpCanCSE)                  // Load from 'double' constant
MACRO_BACKEND_ONLY(     LdC_F8_R8,          Empty,          OpCanCSE)                  // Load 'double' constant
MACRO_BACKEND_ONLY(     LdIndir,            Empty,          OpTempNumberSources|OpTempObjectSources|OpCanCSE)

MACRO_WMS(              ChkUndecl,                  Reg1,           OpSideEffect)

MACRO_WMS(              EnsureNoRootFld,            ElementRootU,   OpSideEffect|OpHasImplicitCall)
MACRO_WMS(              EnsureNoRootRedeclFld,      ElementRootU,   OpSideEffect|OpHasImplicitCall)
MACRO_WMS(              ScopedEnsureNoRedeclFld,    ElementScopedC, OpSideEffect|OpHasImplicitCall)

MACRO_WMS(              InitUndecl,                 Reg1,           OpCanCSE)
//MACRO_WMS(              InitUndeclSlot,             Reg1Unsigned1,  None)
// TODO: Change InitUndeclLetFld and InitUndeclConstFld to ElementU layouts since they do not use their inline cache
MACRO_WMS(              InitUndeclLetFld,           ElementPIndexed,OpByteCodeOnly|OpSideEffect|OpHasImplicitCall)
MACRO_EXTEND_WMS(       InitUndeclLocalLetFld,      ElementP,       OpByteCodeOnly|OpSideEffect|OpHasImplicitCall)
MACRO_WMS(              InitUndeclRootLetFld,       ElementRootU,   OpSideEffect|OpHasImplicitCall)
MACRO_WMS(              InitUndeclConstFld,         ElementPIndexed,OpByteCodeOnly|OpSideEffect|OpHasImplicitCall)
MACRO_EXTEND_WMS(       InitUndeclLocalConstFld,    ElementP,       OpByteCodeOnly|OpSideEffect|OpHasImplicitCall)
MACRO_WMS(              InitUndeclRootConstFld,     ElementRootU,   OpSideEffect|OpHasImplicitCall)
MACRO_EXTEND_WMS(       InitUndeclConsoleLetFld,    ElementScopedU, OpSideEffect|OpHasImplicitCall)
MACRO_EXTEND_WMS(       InitUndeclConsoleConstFld,  ElementScopedU, OpSideEffect|OpHasImplicitCall)
MACRO_WMS(              InitConst,                  Reg2,           OpTempNumberTransfer|OpTempObjectTransfer|OpNonIntTransfer|OpCanCSE)    // Create and initialize 'const' as property of global object
MACRO_WMS(              InitConstSlot,              ElementSlot,    None)
MACRO_WMS(              InitLetFld,                 ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_WMS(              InitRootLetFld,             ElementRootCP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_WMS(              InitConstFld,               ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_WMS(              InitRootConstFld,           ElementRootCP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_WMS(              InitClassMember,            ElementCP,      OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Class member
MACRO_EXTEND_WMS(       InitClassMemberComputedName,ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Class member with computed property name
MACRO_EXTEND_WMS(       InitClassMemberSet,         ElementC,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Class member in set syntax
MACRO_EXTEND_WMS(       InitClassMemberGet,         ElementC,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Class member in get syntax
MACRO_EXTEND_WMS(       InitClassMemberSetComputedName,ElementI,    OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Class member in set syntax with computed property name
MACRO_EXTEND_WMS(       InitClassMemberGetComputedName,ElementI,    OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Class member in get syntax with computed property name
MACRO_EXTEND_WMS(       BrOnClassConstructor,       BrReg1,         None)               // Branch if argument is a class constructor

MACRO_BACKEND_ONLY(     ArgIn_A,                    Empty,          OpSideEffect)       // Copy from "in slot" to "local slot", unchecked
MACRO_WMS(              ArgIn0,                     Reg1,           OpByteCodeOnly)     // Copy from "in slot" to "local slot", unchecked
MACRO_BACKEND_ONLY(     ArgIn_Rest,                 Empty,          OpSideEffect)
MACRO_WMS_PROFILED(     ArgOut_A,                   Arg,            OpSideEffect)       // Copy from "local slot" to "out slot"
MACRO_BACKEND_ONLY(     ArgOut_A_Inline,            Empty,          OpSideEffect|OpTempNumberSources|OpTempObjectSources)   // Copy from "local slot" to "out slot"
MACRO_BACKEND_ONLY(     ArgOut_A_InlineBuiltIn,     Empty,          OpSideEffect|OpTempNumberTransfer|OpTempObjectTransfer) // Used as virtual ArgOut for inline built-ins (Math.sin, etc). Removed in lowerer.
MACRO_BACKEND_ONLY(     ArgOut_A_InlineSpecialized, Empty,          OpSideEffect)       // Used as virtual ArgOut to keep function object around for inlining built-ins by direct call to helpers.
MACRO_WMS(              ArgOut_ANonVar,             Arg,            OpByteCodeOnly|OpSideEffect) // Copy from "local slot" to "out slot"
MACRO_WMS(              ArgOut_Env,                 ArgNoSrc,       OpByteCodeOnly|OpSideEffect) // Copy from "local slot" to "out slot"
MACRO_BACKEND_ONLY(     ArgOut_A_Dynamic,           Empty,          OpSideEffect)       // Copy from "local slot" to "out slot"
MACRO_BACKEND_ONLY(     ArgOut_A_FromStackArgs,     Empty,          OpSideEffect)       // Copy from "local slot" to "out slot"
MACRO_BACKEND_ONLY(     ArgOut_A_FixupForStackArgs, Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     ArgOut_A_SpreadArg,         Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     ArgOutAsmJsI_A,             Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     ArgOutAsmJsE_A,             Empty,          OpSideEffect)
MACRO_WMS(              Delete_A,                   Reg2,           OpSideEffect|OpPostOpDbgBailOut)        // Delete Var

// Object operations
MACRO_WMS_PROFILED_OP(  LdFld,                ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)    // Load from ScriptObject instance's direct field
MACRO_WMS_PROFILED_OP(  LdLocalFld,           ElementP,       OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)    // Load from ScriptObject instance's direct field
MACRO_WMS(              LdEnvObj,             ElementSlotI1,  OpTempNumberSources)
MACRO_EXTEND_WMS_AND_PROFILED_OP(LdSuperFld,  ElementC2,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)    // Load from ScriptObject super instance's direct field
MACRO_WMS_PROFILED_OP(  LdFldForTypeOf,       ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)
MACRO_EXTEND_WMS_AND_PROFILED_OP(LdRootFldForTypeOf, ElementRootCP, OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)

MACRO_WMS_PROFILED_OP(  LdFldForCallApplyTarget,  ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)
MACRO_WMS_PROFILED_OP(LdRootFld,              ElementRootCP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)    // Load from ScriptObject instance's direct field (access to let/const on root object)
MACRO_WMS_PROFILED_OP(LdMethodFld,            ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)    // Load call target from ScriptObject instance's direct field
MACRO_EXTEND_WMS_AND_PROFILED_OP(LdLocalMethodFld, ElementP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpCanLoadFixedFields)    // Load call target from ScriptObject instance's direct field
MACRO_BACKEND_ONLY(     LdMethodFldPolyInlineMiss, ElementCP, OpSideEffect|OpHasImplicitCall|OpDoNotTransfer|OpPostOpDbgBailOut)                        // Load call target from ScriptObject instance's direct field, when the call target is neither of
                                                                                                                                                        // the ones we inlined using fixed methods, at a polymorphic call site,
                                                                                                                                                        // but don't allow it to participate in any obj type spec optimizations,
                                                                                                                                                        // as it will always result in a helper call.
MACRO_WMS_PROFILED_OP(  LdRootMethodFld,      ElementRootCP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Load call target from ScriptObject instance's direct field (access to let/const on root object)
MACRO_WMS_PROFILED_OP(  StFld,                ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Store into ScriptObject instance's direct field
MACRO_EXTEND_WMS_AND_PROFILED_OP(StSuperFld,  ElementC2,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Store into ScriptObject super instance's direct field
MACRO_WMS_PROFILED_OP(  StRootFld,            ElementRootCP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Store into ScriptObject instance's direct field (access to let/const on root object)
MACRO_WMS_PROFILED_OP(  StLocalFld,           ElementP,       OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Store into local activation object
MACRO_WMS_PROFILED_OP(  StFldStrict,          ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Store into ScriptObject instance's direct field (strict mode, a.x = ...)
MACRO_WMS_PROFILED_OP(  StRootFldStrict,      ElementRootCP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Store into ScriptObject instance's direct field (strict mode, x = ..., access to let/const on root object)
MACRO_WMS_PROFILED_OP(  InitFld,              ElementCP,      OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_WMS_PROFILED_OP(  InitLocalFld,         ElementP,       OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_EXTEND_WMS(       InitLocalLetFld,      ElementP,       OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_EXTEND_WMS(       InitInnerFld,         ElementPIndexed,OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_EXTEND_WMS(       InitInnerLetFld,      ElementPIndexed,OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_WMS_PROFILED_OP(  InitRootFld,          ElementRootCP,  OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Declare a property with an initial value
MACRO_BACKEND_ONLY(     LdMethodFromFlags,          ElementCP,      OpFastFldInstr|OpCanCSE)

MACRO_WMS(              DeleteFld,                  ElementC,       OpSideEffect|OpHasImplicitCall|OpDoNotTransfer|OpPostOpDbgBailOut)  // Remove a property
MACRO_EXTEND_WMS(       DeleteLocalFld,             ElementU,       OpSideEffect|OpHasImplicitCall|OpDoNotTransfer|OpPostOpDbgBailOut)  // Remove a property
MACRO_WMS(              DeleteRootFld,              ElementC,       OpSideEffect|OpHasImplicitCall|OpDoNotTransfer|OpPostOpDbgBailOut)  // Remove a property (access to let/const on root object)
MACRO_WMS(              DeleteFldStrict,            ElementC,       OpSideEffect|OpHasImplicitCall|OpDoNotTransfer|OpPostOpDbgBailOut)  // Remove a property in strict mode
MACRO_WMS(              DeleteRootFldStrict,        ElementC,       OpSideEffect|OpHasImplicitCall|OpDoNotTransfer|OpPostOpDbgBailOut)  // Remove a property in strict mode (access to let/const on root object)
MACRO_WMS(              ScopedLdFld,                ElementP,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Load from function's scope stack
MACRO_EXTEND_WMS(       ScopedLdFldForTypeOf,       ElementP,       OpSideEffect|OpHasImplicitCall| OpPostOpDbgBailOut)                 // Load from function's scope stack for Typeof of a property
MACRO_WMS(              ScopedLdMethodFld,          ElementCP,      OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Load call target from ScriptObject instance's direct field, but either scope object or root load from root object
MACRO_WMS(              ScopedLdInst,               ElementScopedC2,OpSideEffect|OpHasImplicitCall)                                     // Load owning instance from function's scope stack (NOTE: HasProperty may call DOM)
MACRO_WMS(              ScopedInitFunc,             ElementScopedC, OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Init on instance on scope stack
MACRO_WMS(              ScopedStFld,                ElementP,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Store to function's scope stack
MACRO_EXTEND_WMS(       ConsoleScopedStFld,         ElementP,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Store to function's scope stack
MACRO_WMS(              ScopedStFldStrict,          ElementP,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Store to function's scope stack
MACRO_WMS(              ScopedDeleteFld,            ElementScopedC, OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Remove a property through a stack of scopes
MACRO_WMS(              ScopedDeleteFldStrict,      ElementScopedC, OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Remove a property through a stack of scopes in strict mode
MACRO_WMS_PROFILED(     LdSlot,                     ElementSlot,    OpTempNumberSources)
MACRO_WMS_PROFILED(     LdEnvSlot,                  ElementSlotI2,  OpTempNumberSources)
MACRO_WMS_PROFILED(     LdInnerSlot,                ElementSlotI2,  OpTempNumberSources)
MACRO_WMS_PROFILED(     LdLocalSlot,                ElementSlotI1,  OpTempNumberSources)
MACRO_BACKEND_ONLY(     LdSlotArr,                  ElementSlot,    OpTempNumberSources)
MACRO_EXTEND_WMS_AND_PROFILED(LdInnerObjSlot,       ElementSlotI2,  OpTempNumberSources)
MACRO_EXTEND_WMS_AND_PROFILED(LdObjSlot,            ElementSlot,    None)
MACRO_EXTEND_WMS_AND_PROFILED(LdLocalObjSlot,       ElementSlotI1,  None)
MACRO_EXTEND_WMS_AND_PROFILED(LdEnvObjSlot,         ElementSlotI2,  None)
MACRO_BACKEND_ONLY(     StSlot,                     ElementSlot,    None)
MACRO_WMS(              StEnvSlot,                  ElementSlotI2,  None)
MACRO_WMS(              StInnerSlot,                ElementSlotI2,  None)
MACRO_WMS(              StLocalSlot,                ElementSlotI1,  None)
MACRO_BACKEND_ONLY(     StSlotChkUndecl,            ElementSlot,    OpSideEffect)
MACRO_EXTEND_WMS(       StEnvSlotChkUndecl,         ElementSlotI2,  OpSideEffect)
MACRO_EXTEND_WMS(       StInnerSlotChkUndecl,       ElementSlotI2,  OpSideEffect)
MACRO_EXTEND_WMS(       StLocalSlotChkUndecl,       ElementSlotI1,  OpSideEffect)
MACRO_EXTEND_WMS(       StObjSlot,                  ElementSlot,    OpSideEffect)
MACRO_EXTEND_WMS(       StInnerObjSlot,             ElementSlotI2,  OpSideEffect)
MACRO_EXTEND_WMS(       StLocalObjSlot,             ElementSlotI1,  OpSideEffect)
MACRO_EXTEND_WMS(       StLocalObjSlotChkUndecl,    ElementSlotI1,  OpSideEffect)
MACRO_EXTEND_WMS(       StEnvObjSlot,               ElementSlotI2,  OpSideEffect)
MACRO_EXTEND_WMS(       StObjSlotChkUndecl,         ElementSlot,    OpSideEffect)
MACRO_EXTEND_WMS(       StInnerObjSlotChkUndecl,    ElementSlotI2,  OpSideEffect)
MACRO_EXTEND_WMS(       StEnvObjSlotChkUndecl,      ElementSlotI2,  OpSideEffect)
MACRO_BACKEND_ONLY(     LdAsmJsSlot,                ElementSlot,    OpTempNumberSources|OpCanCSE)
MACRO_BACKEND_ONLY(     StAsmJsSlot,                ElementSlot,    None)
#ifndef FLOAT_VAR
MACRO_BACKEND_ONLY(     StSlotBoxTemp,              Empty,          OpSideEffect|OpTempNumberSources)
#endif
MACRO_WMS_PROFILED(     LdElemI_A,              ElementI,       OpHasImplicitCall|OpCanCSE|OpPostOpDbgBailOut)          // Load from instance's indirect element / field, checked
MACRO_WMS(              LdMethodElem,           ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)      // Load call target from instance's indirect element / field, checked
MACRO_WMS_PROFILED(     StElemI_A,              ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)      // Store into instance's indirect element / field, checked
MACRO_WMS_PROFILED(     StElemI_A_Strict,       ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)      // Store into instance's indirect element / field, checked
MACRO_BACKEND_ONLY(     StInt8ArrViewElem,      ElementI,       OpSideEffect        )       // Store Int8 into typed array view
MACRO_BACKEND_ONLY(     StUInt8ArrViewElem,     ElementI,       OpSideEffect        )       // Store UInt8 into typed array view
MACRO_BACKEND_ONLY(     StInt16ArrViewElem,     ElementI,       OpSideEffect        )       // Store Int16 into typed array view
MACRO_BACKEND_ONLY(     StUInt16ArrViewElem,    ElementI,       OpSideEffect        )       // Store UInt16 into typed array view
MACRO_BACKEND_ONLY(     StInt32ArrViewElem,     ElementI,       OpSideEffect        )       // Store Int32 into typed array view
MACRO_BACKEND_ONLY(     StUInt32ArrViewElem,    ElementI,       OpSideEffect        )       // Store UInt32 into typed array view
MACRO_BACKEND_ONLY(     StFloat32ArrViewElem,   ElementI,       OpSideEffect        )       // Store Float32 into typed array view
MACRO_BACKEND_ONLY(     StFloat64ArrViewElem,   ElementI,       OpSideEffect        )       // Store Float64 into typed array view
MACRO_BACKEND_ONLY(     LdFloat32ArrViewElem,   ElementI,       OpCanCSE            )       // load Float32 typed array view
MACRO_BACKEND_ONLY(     LdFloat64ArrViewElem,   ElementI,       OpCanCSE            )       // load Float64 typed array view
MACRO_BACKEND_ONLY(     LdInt8ArrViewElem,      ElementI,       OpCanCSE            )       // load Int8 from typed array view
MACRO_BACKEND_ONLY(     LdInt16ArrViewElem,     ElementI,       OpCanCSE            )       // load Int16 from typed array view
MACRO_BACKEND_ONLY(     LdUInt8ArrViewElem,     ElementI,       OpCanCSE            )       // load UInt8 from typed array view
MACRO_BACKEND_ONLY(     LdUInt16ArrViewElem,    ElementI,       OpCanCSE            )       // load UInt16 from typed array view
MACRO_BACKEND_ONLY(     LdInt32ArrViewElem,     ElementI,       OpCanCSE            )       // load Int32 from typed array view
MACRO_BACKEND_ONLY(     LdUInt32ArrViewElem,    ElementI,       OpCanCSE            )       // load UInt32 from typed array view
MACRO_BACKEND_ONLY(     Memset,                 ElementI,       OpSideEffect)
MACRO_BACKEND_ONLY(     Memcopy,                ElementI,       OpSideEffect)
MACRO_BACKEND_ONLY(     ArrayDetachedCheck,     Reg1,           None)   // ensures that an ArrayBuffer has not been detached
MACRO_WMS(              StArrItemI_CI4,         ElementUnsigned1,      OpSideEffect)
MACRO_WMS(              StArrItemC_CI4,         ElementUnsigned1,      OpSideEffect)
MACRO_WMS(              LdArrHead,              Reg2,           OpTempObjectSources)
MACRO_BACKEND_ONLY(     BoundCheck,             Empty,          OpTempNumberSources|OpTempObjectSources)
MACRO_BACKEND_ONLY(     UnsignedBoundCheck,     Empty,          OpTempNumberSources|OpTempObjectSources)
MACRO_WMS(              StArrInlineItem_CI4,    ElementUnsigned1,      OpSideEffect)
MACRO_WMS(              StArrSegItem_CI4,       ElementUnsigned1,      OpSideEffect)
MACRO(                  StArrSegItem_A,         Auxiliary,      OpSideEffect)
MACRO_WMS(              DeleteElemI_A,          ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Remove from instance's indirect element / field, checked
MACRO_WMS(              DeleteElemIStrict_A,    ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Remove from instance's indirect element / field, checked
MACRO_EXTEND_WMS(       InitSetFld,             ElementC,       OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Set in Object Literal Syntax {set prop(args){}};
MACRO_EXTEND_WMS(       InitGetFld,             ElementC,       OpSideEffect|OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut)   // Get in Object Literal Syntax {get prop(){}};
MACRO_EXTEND_WMS(       InitSetElemI,           ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Set in Object Literal Syntax {set [expr](args){}};
MACRO_EXTEND_WMS(       InitGetElemI,           ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Get in Object Literal Syntax {get [expr](args){}};
MACRO_EXTEND_WMS(       InitComputedProperty,   ElementI,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Data property in Object Literal Syntax { [expr] : expr};
MACRO_EXTEND_WMS(       InitProto,              ElementC,       OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)                  // Declare __proto__ in Object Literal Syntax {__proto__: ...}

MACRO_WMS_PROFILED(     LdLen_A,            Reg2,           OpHasImplicitCall|OpFastFldInstr|OpPostOpDbgBailOut|OpTempObjectSources)       // Push array / string length

MACRO_WMS(              LdUndef,            Reg1,           OpByteCodeOnly|OpCanCSE)       // Load 'undefined'
MACRO_WMS(              LdNaN,              Reg1,           OpByteCodeOnly|OpCanCSE)       // Load 'NaN'
MACRO_WMS(              LdInfinity,         Reg1,           OpByteCodeOnly|OpCanCSE)       // Load 'Infinity'
MACRO_WMS(              LdTrue,             Reg1,           OpByteCodeOnly|OpCanCSE)       // Load 'true' boolean primitive
MACRO_WMS(              LdFalse,            Reg1,           OpByteCodeOnly|OpCanCSE)       // Load 'false' boolean primitive
MACRO_BACKEND_ONLY(     LdEnv,              Reg1,           OpCanCSE)       // Load the optional FixedSizeArray environment for closures
MACRO_BACKEND_ONLY(     LdAsmJsEnv,         Reg1,           OpCanCSE)       // Load the asm.js memory

MACRO_WMS(              LdArgCnt,           Reg1,           None)           // Load the argument count from the current function

MACRO_BACKEND_ONLY(     InitLoopBodyCount,  Reg1,           None)           // fake instruction for loop body counter init
MACRO_BACKEND_ONLY(     IncrLoopBodyCount,  Reg2,           None)           // fake instruction for loop body counter increment
MACRO_BACKEND_ONLY(     StLoopBodyCount,    Reg1,           OpSideEffect)   // fake instruction for loop body counter store to loopheader profiledLoopCounter

MACRO_WMS(              LdHeapArguments,    Reg2,           OpSideEffect)   // Load the heap-based "arguments" object
MACRO_WMS(              LdLetHeapArguments, Reg2,           OpSideEffect)   // Load the heap-based "arguments" object (formals are let-like instead of var-like)
MACRO_BACKEND_ONLY(     LdArgumentsFromStack,Reg1,          None)           // Load the heap-based "arguments" object even if it is null (Loads from meta arguments location for inlinee as well).
MACRO_WMS(              LdHeapArgsCached,   Reg1,           OpSideEffect)   // Load the heap-based "arguments" object in a cached scope
MACRO_EXTEND_WMS(       LdLetHeapArgsCached,Reg1,           OpSideEffect)   // Load the heap-based "arguments" object in a cached scope (formals are let-like instead of var-like)
MACRO_EXTEND_WMS(       LdStackArgPtr,      Reg1,           OpSideEffect)   // Load the address of the base of the input parameter area
MACRO_WMS_PROFILED_OP(  LdThis,       Reg2Int1,       OpHasImplicitCall|OpTempNumberTransfer)        // Load this object     (NOTE: TryLoadRoot exit scripts on host dispatch, but otherwise, no side effect)
MACRO_WMS_PROFILED_OP(  StrictLdThis, Reg2,           OpHasImplicitCall|OpTempNumberTransfer)        // Load this object in strict mode
MACRO_BACKEND_ONLY(     CheckThis,          Reg1,           OpCanCSE|OpBailOutRec)
MACRO_BACKEND_ONLY(     StrictCheckThis,    Reg1,           OpCanCSE|OpBailOutRec)
MACRO_BACKEND_ONLY(     LdHandlerScope,     Reg1,           OpHasImplicitCall)     // Load a scope stack for an event handler (both "this" and parent scopes)
MACRO_BACKEND_ONLY(     LdFrameDisplay,     Reg3,           None)           // Set up a frame display for this function and its parent frames
#if DBG
MACRO_BACKEND_ONLY(     LdFrameDisplayNoParent,Reg1,        None)           // Set up a frame display for this function and its parent frames
#endif
MACRO_WMS(              LdInnerFrameDisplay,Reg3,           None)        // Set up a frame display for this function and its parent frames -- this is for an inner scope, not the function-level scope
MACRO_WMS(              LdInnerFrameDisplayNoParent,Reg2,   None)        // Set up a frame display for this function and its parent frames -- this is for an inner scope, not the function-level scope
MACRO_WMS(              LdIndexedFrameDisplay,Reg2Int1,         None)        // Set up a frame display for this function and its parent frames -- this is for an inner scope, not the function-level scope
MACRO_WMS(              LdIndexedFrameDisplayNoParent,Reg1Unsigned1, None)        // Set up a frame display for this function and its parent frames -- this is for an inner scope, not the function-level scope
MACRO_WMS(              LdFuncExprFrameDisplay,Reg2,        None)
MACRO_BACKEND_ONLY(     NewStackFrameDisplay,Reg3,          None)           // Set up a frame display allocated on the stack
MACRO_WMS(              IsIn,               Reg3,           OpSideEffect|OpCallsValueOf|OpHasImplicitCall|OpPostOpDbgBailOut)        // "x in y"  (NOTE: calls valueOf for the index
MACRO_WMS(              LdArgumentsFromFrame,Reg1,          None)           // Load the argument object from frame
MACRO_WMS(              LdElemUndef,        ElementU,       OpSideEffect)   // Load 'undefined' to instance.property if not already present
MACRO_EXTEND_WMS(       LdLocalElemUndef,   ElementRootU,   OpSideEffect)   // Load 'undefined' to instance.property if not already present
MACRO_WMS(              LdElemUndefScoped,  ElementScopedU, OpSideEffect)   // Load 'undefined' to [env].property if not already present in the scope
MACRO_WMS(              LdFuncExpr,         Reg1,           None)           // Load the function expression to its location
MACRO(                  LdPropIds,          Auxiliary,      None)           // Load the property id array
MACRO_WMS(              StFuncExpr,         ElementC,       OpSideEffect)   // Store the function expression to an activation object with attributes
MACRO_WMS(              StLocalFuncExpr,    ElementU,       OpSideEffect)   // Store the function expression to the local activation object with attributes
MACRO_EXTEND_WMS(       LdNewTarget,        Reg1,           None)           // Load new.target in an ordinary function call or construct call
MACRO_EXTEND(           ChkNewCallFlag,     Empty,          OpSideEffect)   // Check to see if the current call has the CallFlags_New flag set and throw if it does not

// Instancing operations
MACRO_EXTEND_WMS(LdCustomSpreadIteratorList, Reg2,          OpHasImplicitCall|OpSideEffect)                 // Runs the iterator on a function spread Argument
MACRO_WMS_PROFILED_INDEX( NewScObject,       CallI,         OpSideEffect|OpUseAllFields|OpCallInstr)        // Create new ScriptObject instance
MACRO_EXTEND_WMS_AND_PROFILED(NewScObjectSpread,   CallIExtended, OpSideEffect|OpUseAllFields|OpCallInstr)  // Create new ScriptObject instance
MACRO_WMS_PROFILED2(    NewScObjArray,      CallI,          OpSideEffect|OpUseAllFields|OpCallInstr)        // Create new ScriptObject instance
MACRO_WMS_PROFILED2(    NewScObjArraySpread, CallIExtended, OpSideEffect|OpUseAllFields|OpCallInstr)        // Create new ScriptObject instance
MACRO(                  NewScObject_A,      Auxiliary,      OpSideEffect|OpUseAllFields)                    // Create new ScriptObject instance passing only constants
MACRO_WMS(              NewScObjectNoCtorFull, Reg2,        OpHasImplicitCall | OpTempObjectCanStoreTemp)   // Create new object that will be used for the 'this' binding in a base class constructor
MACRO_BACKEND_ONLY(     NewScObjectNoCtor,  Empty,          OpHasImplicitCall | OpTempObjectCanStoreTemp)   // Create new object that will be passed into a constructor
MACRO_BACKEND_ONLY(     GetNewScObject,     Empty,          OpTempObjectTransfer)                           // Determine which object to finally use as the result of NewScObject (object passed into constructor as 'this', or object returned by constructor)
MACRO_BACKEND_ONLY(     UpdateNewScObjectCache, Empty,      None)                                           // Update the cache used for NewScObject
MACRO_WMS(              NewScObjectSimple,  Reg1,           OpTempObjectCanStoreTemp)
MACRO(                  NewScObjectLiteral, Auxiliary,      OpSideEffect|OpTempObjectCanStoreTemp)          // Create new ScriptObject instance with no constructor or arguments
MACRO_WMS_PROFILED(     NewScArray,         Reg1Unsigned1,  OpSideEffect|OpTempObjectProducing)             // Create new ScriptArray instance
MACRO_WMS(              NewScArrayWithMissingValues, Reg1Unsigned1,    OpSideEffect|OpTempObjectProducing)  // Create new ScriptArray instance
MACRO_PROFILED(         NewScIntArray,      Auxiliary,      OpSideEffect|OpTempObjectProducing)             // Create new ScriptArray instance
MACRO_PROFILED(         NewScFltArray,      Auxiliary,      OpSideEffect|OpTempObjectProducing)             // Create new ScriptArray instance
MACRO_EXTEND_WMS(       InitClass,          Class,          OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)

MACRO_WMS(              NewScFunc,          ElementSlotI1,  OpSideEffect)   // Create new ScriptFunction instance
MACRO_WMS(              NewScGenFunc,       ElementSlotI1,  OpSideEffect)   // Create new JavascriptGeneratorFunction instance
MACRO_WMS(              NewStackScFunc,     ElementSlotI1,  OpSideEffect|OpByteCodeOnly)  // Create new ScriptFunction instance
MACRO_EXTEND_WMS(       NewInnerScFunc,     ElementSlot,    OpSideEffect)   // Create new ScriptFunction instance
MACRO_EXTEND_WMS(       NewInnerScGenFunc,  ElementSlot,    OpSideEffect)   // Create new JavascriptGeneratorFunction instance
MACRO_EXTEND_WMS(       NewInnerStackScFunc,ElementSlot,    OpSideEffect|OpByteCodeOnly)  // Create new ScriptFunction instance
MACRO_BACKEND_ONLY(     NewScopeObject,     Reg1,           None)                       // Create new NewScopeObject
MACRO_BACKEND_ONLY(     InitCachedScope,    Reg2Aux,        OpSideEffect)                   // Retrieve cached scope; create if not cached
MACRO_BACKEND_ONLY(     InitLetCachedScope, Reg2Aux,        OpSideEffect)                   // Retrieve cached scope; create if not cached (formals are let-like instead of var-like)
MACRO(                  InitCachedFuncs,    AuxNoReg,       OpSideEffect)
MACRO_WMS(              GetCachedFunc,      Reg1Unsigned1,  None)
MACRO(                  CommitScope,        AuxNoReg,       OpSideEffect)   // Mark the cached scope object as committed on exit from the function
MACRO_WMS(              InvalCachedScope,   Unsigned1,      OpSideEffect)
MACRO_WMS(              NewPseudoScope,     Unsigned1,      None)           // Create new scope that can't take normal var inits
MACRO_WMS(              NewBlockScope,      Unsigned1,      None)           // Create new scope that takes only block-scoped inits
MACRO_WMS(              CloneBlockScope,    Unsigned1,      OpSideEffect)   // Clone existing block scope in place for for-loop iterations
MACRO_BACKEND_ONLY(     NewScopeSlots,      Reg1Unsigned1,  None)
MACRO_BACKEND_ONLY(     NewStackScopeSlots, Reg1,           None)
MACRO_BACKEND_ONLY(     InitLocalClosure,   Reg1,           None)
MACRO_WMS(              NewInnerScopeSlots, Reg3,           None)
MACRO_WMS(              CloneInnerScopeSlots, Unsigned1,    OpSideEffect)   // Clone existing inner scope slots in place for for-loop iterations
MACRO_BACKEND_ONLY(     NewScopeSlotsWithoutPropIds, Reg1Int2, None)
MACRO_WMS(              NewRegEx,           Reg1Unsigned1,  OpTempObjectCanStoreTemp|OpSideEffect)              // Create a new RegEx expression
MACRO_WMS(              IsInst,             Reg3C,          OpSideEffect|OpHasImplicitCall|OpPostOpDbgBailOut)  // instanceof() - SideEffect: can throw...

// Collection operations
MACRO_WMS(              GetForInEnumerator, Reg2,           OpSideEffect|OpHasImplicitCall)         // Get enumerator from collection
MACRO_WMS(              BrOnEmpty,          BrReg2,         OpSideEffect|OpHasImplicitCall)         // Move to next item; return value if not NULL, otherwise branch
MACRO_BACKEND_ONLY (    BrOnNotEmpty,       BrReg2,         OpSideEffect|OpHasImplicitCall)         // Move to next item; return true if done
MACRO_WMS(              ReleaseForInEnumerator,  Reg1,      OpSideEffect|OpHasImplicitCall)         // Release enumerator

MACRO(                  TryCatch,           Br,             OpSideEffect)
MACRO(                  TryFinally,         Br,             OpSideEffect|OpPostOpDbgBailOut)
MACRO_EXTEND_WMS(       TryFinallyWithYield, BrReg2,         OpSideEffect|OpPostOpDbgBailOut)
MACRO_WMS(              Catch,              Reg1,           OpSideEffect)
MACRO_EXTEND(           ResumeCatch,        Empty,          OpSideEffect)
MACRO_EXTEND_WMS(       ResumeFinally,      BrReg2,         OpSideEffect)
MACRO(                  LeaveNull,          Empty,          OpSideEffect|OpNoFallThrough)
MACRO(                  Leave,              Empty,          OpSideEffect|OpNoFallThrough)

MACRO_BACKEND_ONLY(     InlineRuntimeTypeError,        W1,             OpSideEffect|OpPostOpDbgBailOut)     // Throws TypeError at runtime.
MACRO_EXTEND(           RuntimeTypeError,              W1,             OpSideEffect|OpPostOpDbgBailOut)     // Throws TypeError at runtime.
MACRO_BACKEND_ONLY(     InlineRuntimeReferenceError,   W1,             OpSideEffect|OpPostOpDbgBailOut)     // Throws ReferenceError at runtime.
MACRO_EXTEND(           RuntimeReferenceError,         W1,             OpSideEffect|OpPostOpDbgBailOut)     // Throws ReferenceError at runtime.


// Dynamic profile opcodes
MACRO_WMS(              LoopBodyStart,          Unsigned1,             OpByteCodeOnly)  // Marks the start of a loop body
MACRO_WMS(              ProfiledLoopStart,      Unsigned1,             OpSideEffect)    // Marks the start of a profiled loop
MACRO_WMS(              ProfiledLoopBodyStart,  Unsigned1,             OpSideEffect)    // Marks the start of a profiled loop body
MACRO_WMS(              ProfiledLoopEnd,        Unsigned1,             OpSideEffect)    // Marks the end of a profiled loop

// The order of this need to be the same as the Call* order
MACRO_WMS(              ProfiledCallI,          ProfiledCallI,         OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)    // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              ProfiledCallIFlags,     ProfiledCallIFlags,    OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)    // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              ProfiledCallIExtended,  ProfiledCallIExtended, OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_WMS(              ProfiledCallIExtendedFlags, ProfiledCallIExtendedFlags, OpByteCodeOnly|OpSideEffect|OpUseAllFields| OpCallInstr)

MACRO_WMS(              ProfiledCallIWithICIndex,          ProfiledCallIWithICIndex,          OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)          // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              ProfiledCallIFlagsWithICIndex,     ProfiledCallIFlagsWithICIndex,     OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)          // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              ProfiledCallIExtendedWithICIndex,  ProfiledCallIExtendedWithICIndex,          OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)  // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              ProfiledCallIExtendedFlagsWithICIndex, ProfiledCallIExtendedFlagsWithICIndex,          OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr)

// The order of this needs to be the same as the Call* order
MACRO_WMS(              ProfiledReturnTypeCallI,          ProfiledCallI,          OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)     // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              ProfiledReturnTypeCallIFlags,     ProfiledCallIFlags,     OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)     // Return <- Call (indirect) Function(ArgCount)
MACRO_WMS(              ProfiledReturnTypeCallIExtended,  ProfiledCallIExtended,  OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr)
MACRO_WMS(              ProfiledReturnTypeCallIExtendedFlags, ProfiledCallIExtendedFlags, OpByteCodeOnly|OpSideEffect|OpUseAllFields|OpCallInstr)


MACRO_EXTEND_WMS(       EmitTmpRegCount,    Unsigned1,      OpByteCodeOnly)
MACRO_WMS(              Unused,             Reg1,           None)

// String operations
MACRO_WMS(              Concat3,            Reg4,           OpByteCodeOnly|OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)
MACRO_WMS(              NewConcatStrMulti,  Reg3B1,         None)       // Although the byte code version include the concat, and has value of/to string, the BE version doesn't
MACRO_BACKEND_ONLY(     NewConcatStrMultiBE, Reg3B1,        OpCanCSE)   // Although the byte code version include the concat, and has value of/to string, the BE version doesn't
MACRO_WMS(              SetConcatStrMultiItem,   Reg2B1,    None)       // Although the byte code version include the concat, and has value of/to string, the BE version doesn't
MACRO_BACKEND_ONLY(     SetConcatStrMultiItemBE, Reg2B1,    OpCanCSE)   // Although the byte code version include the concat, and has value of/to string, the BE version doesn't
MACRO_WMS(              SetConcatStrMultiItem2,  Reg3B1,         None)  // Although the byte code version include the concat, and has value of/to string, the BE version doesn't
MACRO_BACKEND_ONLY(     LdStr,              Empty,          OpTempNumberProducing|OpCanCSE)                 // Load string literal
MACRO_BACKEND_ONLY(     CloneStr,           Empty,          OpTempNumberSources | OpTempNumberProducing)    // Load string literal


// Operation ToString(str) if str != null or str != undefined
MACRO_BACKEND_ONLY(     Coerse_Str, Empty, OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpPostOpDbgBailOut)

// Operation CreateRegex(regex)
MACRO_BACKEND_ONLY(     Coerse_Regex, Empty, OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempNumberTransfer|OpPostOpDbgBailOut)

// Operation CreateRegex(regex) or CoerseStr(arg)
MACRO_BACKEND_ONLY(     Coerse_StrOrRegex, Empty, OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempNumberTransfer|OpPostOpDbgBailOut)

// Operation ToString(ToPrimtive(src1))
MACRO_BACKEND_ONLY(     Conv_PrimStr,       Empty,          OpCallsValueOf|OpHasImplicitCall|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpPostOpDbgBailOut)

// The following are for lowering; currently the specified layout is not used
MACRO_BACKEND_ONLY(     StElemC,            ElementC,       OpSideEffect)                   // Store into Array instance's direct element, unchecked
MACRO_BACKEND_ONLY(     StArrSegElemC,      ElementC,       OpSideEffect)                   // Store into Array segment's direct element, unchecked
MACRO_EXTEND(           Label,              Empty,          OpSideEffect)                   // Label (branch target)
MACRO_BACKEND_ONLY(     CallHelper,         Empty,          OpSideEffect|OpUseAllFields)    // Call a helper function

MACRO_EXTEND(           FunctionEntry,      Empty,          OpSideEffect)                   // Marks the start of a function
MACRO_EXTEND(           FunctionExit,       Empty,          OpSideEffect|OpNoFallThrough)   // Marks the end of a function
MACRO_EXTEND(           StatementBoundary,  Empty,          None)                           // Marks the start or end of a statement

MACRO_BACKEND_ONLY(     BailOut,                     Empty,          OpSideEffect|OpBailOutRec)
MACRO_BACKEND_ONLY(     BailOnEqual,                 Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     BailOnNotEqual,              Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     BailOnNegative,              Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     BailOnNotStackArgs,          Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE)    // Bail out if not stack args or actuals exceed InlineeCallInfo::MaxInlineeArgoutCount (15)
MACRO_BACKEND_ONLY(     BailOnNotSpreadable,         Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     BailOnNotPolymorphicInlinee, Empty,          OpBailOutRec|OpTempNumberSources)
MACRO_BACKEND_ONLY(     BailTarget,                  Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources)
MACRO_BACKEND_ONLY(     BailOnNoProfile,             Empty,          OpBailOutRec|OpDeadFallThrough)
MACRO_BACKEND_ONLY(     BailOnNoSimdTypeSpec,        Empty,          OpBailOutRec|OpDeadFallThrough)
MACRO_BACKEND_ONLY(     BailOnNotObject,             Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpTempObjectSources)
MACRO_BACKEND_ONLY(     BailOnNotArray,              Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE|OpTempObjectSources)
MACRO_BACKEND_ONLY(     BailForDebugger,             Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpSideEffect)    // Bail out so that we can continue the function under debugger. Disable optimizations for this instr so that it's not moved.
MACRO_BACKEND_ONLY(     BailOnNotBuiltIn,            Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     BailOnException,             Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpDeadFallThrough)
MACRO_BACKEND_ONLY(     BailOnTaggedValue,           Empty,          OpBailOutRec|OpTempNumberSources|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     BytecodeArgOutCapture,       Empty,          OpTempNumberTransfer|OpTempObjectTransfer|OpNonIntTransfer) // Represents snapshotting of bytecode ArgOut_A in backend for purpose of bailout
MACRO_BACKEND_ONLY(     BytecodeArgOutUse,           Empty,          OpTempNumberSources | OpTempObjectSources) // Represents bytecode ArgOut_A use in the backend to keep args alive for the globopt

MACRO_BACKEND_ONLY(     ByteCodeUses,       Empty,          OpSideEffect|OpTempNumberSources|OpTempObjectSources)
MACRO_BACKEND_ONLY(     BailOutStackRestore,Empty,          OpSideEffect)

MACRO_BACKEND_ONLY(     NoImplicitCallUses, Empty,          None)
MACRO_BACKEND_ONLY(     NoIntOverflowBoundary, Empty,       None)

MACRO_BACKEND_ONLY(     EndCallForPolymorphicInlinee, Empty,  OpSideEffect|OpDeadFallThrough)
MACRO_BACKEND_ONLY(     InlineeStart,       Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     InlineeEnd,         Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     InlineeMetaArg,     Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     InlineBuiltInStart, Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     InlineBuiltInEnd,   Empty,          OpSideEffect)
MACRO_BACKEND_ONLY(     InlineNonTrackingBuiltInEnd, Empty, OpSideEffect)
MACRO_BACKEND_ONLY(     InlineMathAcos,     Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathAsin,     Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathAtan,     Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathAtan2,    Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathCos,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathExp,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathLog,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathPow,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathSin,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathSqrt,     Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineMathTan,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpProducesNumber|OpCanCSE)

// IE11 inline built-ins
// TODO: put these upfront so that all built-ins are sorted.
MACRO_BACKEND_ONLY(     InlineMathAbs,       Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathClz32,     Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathCeil,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpBailOutRec|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathFloor,     Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpBailOutRec|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathMax,       Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathImul,      Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathMin,       Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathRandom,    Empty,          OpSideEffect|OpInlinableBuiltIn|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathRound,     Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpBailOutRec|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineMathFround,    Empty,          OpInlinableBuiltIn|OpTempNumberSources|OpCanCSE|OpProducesNumber)
MACRO_BACKEND_ONLY(     InlineStringCharAt,  Empty,          OpInlinableBuiltIn|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineStringCharCodeAt, Empty,       OpInlinableBuiltIn|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineStringCodePointAt, Empty,      OpInlinableBuiltIn|OpCanCSE)
MACRO_BACKEND_ONLY(     InlineArrayPop,      Empty,          OpSideEffect|OpInlinableBuiltIn|OpHasImplicitCall)
MACRO_BACKEND_ONLY(     InlineArrayPush,     Empty,          OpSideEffect|OpInlinableBuiltIn|OpHasImplicitCall)
MACRO_BACKEND_ONLY(     InlineFunctionApply, Empty,          OpSideEffect|OpInlinableBuiltIn)
MACRO_BACKEND_ONLY(     InlineFunctionCall,  Empty,          OpSideEffect|OpInlinableBuiltIn)
MACRO_BACKEND_ONLY(     InlineRegExpExec,    Empty,          OpSideEffect|OpInlinableBuiltIn)

MACRO_BACKEND_ONLY(     CallIFixed,          Empty,          OpSideEffect|OpUseAllFields|OpCallInstr|OpInlineCallInstr)
MACRO_BACKEND_ONLY(     CheckFixedFld,       Empty,          OpFastFldInstr|OpTempObjectSources|OpCanCSE)
MACRO_BACKEND_ONLY(     CheckPropertyGuardAndLoadType,  Empty,          OpFastFldInstr|OpTempObjectSources|OpDoNotTransfer)
MACRO_BACKEND_ONLY(     CheckObjType,        Empty,          OpFastFldInstr|OpTempObjectSources)
MACRO_BACKEND_ONLY(     AdjustObjType,       Empty,          OpSideEffect)

// Edge inline built-ins
#ifdef ENABLE_DOM_FAST_PATH
MACRO_BACKEND_ONLY(     DOMFastPathGetter,   Empty,          OpCanCSE)  // unlike other builtins, we don't know the return type
MACRO_BACKEND_ONLY(     DOMFastPathSetter,   Empty,          OpSideEffect)
#endif
MACRO_BACKEND_ONLY(     ExtendArg_A,         Empty,          OpCanCSE)  // extend a list of arguments to be passed to functions.

#ifdef _M_X64
MACRO_BACKEND_ONLY(     PrologStart,        Empty,          None)
MACRO_BACKEND_ONLY(     PrologEnd,          Empty,          None)
MACRO_BACKEND_ONLY(     LdArgSize,          Empty,          None)
MACRO_BACKEND_ONLY(     LdSpillSize,        Empty,          None)
#endif

#if DBG_DUMP
MACRO_BACKEND_ONLY(     LdRoot,             Empty,          None)       // Load root object
MACRO_BACKEND_ONLY(     LdModuleRoot,       Empty,          None)       // Load module root with given moduleID
MACRO_BACKEND_ONLY(     LdNullDisplay,      Empty,          None)       // Load the null frame display
MACRO_BACKEND_ONLY(     LdStrictNullDisplay,Empty,          None)       // Load the strict null frame display
#endif

MACRO(                  SpreadArrayLiteral, Reg2Aux,        OpSideEffect|OpHasImplicitCall)
MACRO_BACKEND_ONLY(     LdSpreadIndices,    Empty,          None)

MACRO_EXTEND_WMS(       ClearAttributes,    ElementU,       None)
MACRO_EXTEND_WMS(       ObjectFreeze,       Reg1,           None)

MACRO_EXTEND_WMS(       LdSuper,            Reg1,           OpSideEffect)
MACRO_EXTEND_WMS(       LdSuperCtor,        Reg1,           OpSideEffect)
MACRO_EXTEND_WMS(       ScopedLdSuper,      Reg1,           OpSideEffect)
MACRO_EXTEND_WMS(       ScopedLdSuperCtor,  Reg1,           OpSideEffect)
MACRO_EXTEND_WMS(       SetHomeObj,         Reg2,           OpSideEffect)

MACRO_BACKEND_ONLY(     BrFncCachedScopeEq, Reg2,           None)
MACRO_BACKEND_ONLY(     BrFncCachedScopeNeq,Reg2,           None)

MACRO_BACKEND_ONLY(     GeneratorResumeJumpTable, Reg1,     OpSideEffect)
MACRO_BACKEND_ONLY(     RestoreOutParam,    Empty,          None)

MACRO_BACKEND_ONLY(     SlotArrayCheck,     Empty,          OpCanCSE)
MACRO_BACKEND_ONLY(     FrameDisplayCheck,  Empty,          OpCanCSE)

// All SIMD ops are backend only for non-asmjs.
#define MACRO_SIMD(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_BACKEND_ONLY(opcode, Empty, OpCodeAttr)
#define MACRO_SIMD_WMS(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_BACKEND_ONLY(opcode, Empty, OpCodeAttr)

#define MACRO_SIMD_BACKEND_ONLY(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr) MACRO_BACKEND_ONLY(opcode, Empty, OpCodeAttr)

#define MACRO_SIMD_EXTEND(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_BACKEND_ONLY(opcode, Empty, OpCodeAttr)
#define MACRO_SIMD_EXTEND_WMS(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr, ...) MACRO_BACKEND_ONLY(opcode, Empty, OpCodeAttr)
#define MACRO_SIMD_BACKEND_ONLY_EXTEND(opcode, asmjsLayout, opCodeAttrAsmJs, OpCodeAttr) MACRO_BACKEND_ONLY(opcode, Empty, OpCodeAttr)

#include "OpCodesSimd.h"

#undef MACRO_WMS_PROFILED
#undef MACRO_WMS_PROFILED2
#undef MACRO_WMS_PROFILED_OPCODE
#undef MACRO_PROFILED
#undef MACRO_DEBUG_WMS
#undef MACRO_DEBUG

// help the caller to undefine all the macros
#undef MACRO
#undef MACRO_WMS
#undef MACRO_EXTEND
#undef MACRO_EXTEND_WMS
#undef MACRO_BACKEND_ONLY
