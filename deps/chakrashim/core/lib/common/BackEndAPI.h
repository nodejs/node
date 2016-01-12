//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if DYNAMIC_INTERPRETER_THUNK
#define DefaultEntryThunk Js::InterpreterStackFrame::DelayDynamicInterpreterThunk
#if _M_X64
#define AsmJsDefaultEntryThunk Js::InterpreterStackFrame::AsmJsDelayDynamicInterpreterThunk
#elif _M_IX86
#define AsmJsDefaultEntryThunk Js::InterpreterStackFrame::DelayDynamicInterpreterThunk
#endif
#else
#define DefaultEntryThunk Js::InterpreterStackFrame::InterpreterThunk
#endif

#define ProfileEntryThunk Js::ScriptContext::DebugProfileProbeThunk

#define DefaultDeferredParsingThunk Js::JavascriptFunction::DeferredParsingThunk
#define ProfileDeferredParsingThunk Js::ScriptContext::ProfileModeDeferredParsingThunk

#define DefaultDeferredDeserializeThunk Js::JavascriptFunction::DeferredDeserializeThunk
#define ProfileDeferredDeserializeThunk Js::ScriptContext::ProfileModeDeferredDeserializeThunk
#if ENABLE_NATIVE_CODEGEN
class NativeCodeGenerator;
class ThreadContext;
struct CodeGenWorkItem;
class NativeCodeData;

class StackSym;
class Func;
struct InlinedFrameLayout;

typedef intptr IntConstType;
typedef uintptr  UIntConstType;
typedef IntMath<intptr>::Type IntConstMath;
typedef double  FloatConstType;

#include "..\Backend\EmitBuffer.h"
#include "..\Backend\InterpreterThunkEmitter.h"
#include "..\Runtime\Bytecode\BackEndOpCodeAttr.h"
#include "..\Backend\BackEndOpCodeAttrAsmJs.h"
#include "..\Backend\CodeGenNumberAllocator.h"
#include "..\Backend\NativeCodeData.h"
#include "..\Backend\JnHelperMethod.h"
#include "..\Backend\IRType.h"
#include "..\Backend\InlineeFrameInfo.h"

NativeCodeGenerator * NewNativeCodeGenerator(Js::ScriptContext * nativeCodeGen);
void DeleteNativeCodeGenerator(NativeCodeGenerator * nativeCodeGen);
void CloseNativeCodeGenerator(NativeCodeGenerator* nativeCodeGen);
bool IsClosedNativeCodeGenerator(NativeCodeGenerator* nativeCodeGen);
void SetProfileModeNativeCodeGen(NativeCodeGenerator *pNativeCodeGen, BOOL fSet);
void UpdateNativeCodeGeneratorForDebugMode(NativeCodeGenerator* nativeCodeGen);

CriticalSection *GetNativeCodeGenCriticalSection(NativeCodeGenerator *pNativeCodeGen);
bool TryReleaseNonHiPriWorkItem(Js::ScriptContext* scriptContext, CodeGenWorkItem* workItem);
void NativeCodeGenEnterScriptStart(NativeCodeGenerator * nativeCodeGen);
bool IsNativeFunctionAddr(Js::ScriptContext *scriptContext, void * address);
void FreeNativeCodeGenAllocation(Js::ScriptContext* scriptContext, void* address);
CodeGenAllocators* GetForegroundAllocator(NativeCodeGenerator * nativeCodeGen, PageAllocator* pageallocator);
void GenerateFunction(NativeCodeGenerator * nativeCodeGen, Js::FunctionBody * functionBody, Js::ScriptFunction * function = NULL);
void GenerateLoopBody(NativeCodeGenerator * nativeCodeGen, Js::FunctionBody * functionBody, Js::LoopHeader * loopHeader, Js::EntryPointInfo* entryPointInfo, uint localCount, Js::Var localSlots[]);
#ifdef ENABLE_PREJIT
void GenerateAllFunctions(NativeCodeGenerator * nativeCodeGen, Js::FunctionBody * fn);
#endif
#ifdef IR_VIEWER
Js::Var RejitIRViewerFunction(NativeCodeGenerator *nativeCodeGen, Js::FunctionBody *fn, Js::ScriptContext *scriptContext);
#endif

BOOL IsIntermediateCodeGenThunk(Js::JavascriptMethod codeAddress);
BOOL IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress);
typedef Js::JavascriptMethod (*CheckCodeGenFunction)(Js::ScriptFunction * function);
CheckCodeGenFunction GetCheckCodeGenFunction(Js::JavascriptMethod codeAddress);

uint GetBailOutRegisterSaveSlotCount();
uint GetBailOutReserveSlotCount();

#if DBG
void CheckIsExecutable(Js::RecyclableObject * function, Js::JavascriptMethod entryPoint);
#endif

#ifdef PROFILE_EXEC
namespace Js
{
class ScriptContextProfiler;
};
void CreateProfilerNativeCodeGen(NativeCodeGenerator * nativeCodeGen, Js::ScriptContextProfiler * profiler);
void ProfilePrintNativeCodeGen(NativeCodeGenerator * nativeCodeGen);
void SetProfilerFromNativeCodeGen(NativeCodeGenerator * toNativeCodeGen, NativeCodeGenerator * fromNativeCodeGen);
#endif

void DeleteNativeCodeData(NativeCodeData * data);
#else
inline BOOL IsIntermediateCodeGenThunk(Js::JavascriptMethod codeAddress) { return false; }
inline BOOL IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress) { return false; }
#endif

#if _M_X64
extern "C" void * amd64_ReturnFromCallWithFakeFrame();
#endif

struct InlinedFrameLayout
{
    Js::InlineeCallInfo     callInfo;
    Js::JavascriptFunction *function;
    Js::Var                 arguments;  // The arguments object.
    //Js::Var               argv[0];    // Here it would be embedded arguments array (callInfo.count elements)
                                        // but can't have 0-size arr in base class, so we define it in derived class.

    Js::Var* GetArguments()
    {
        return (Js::Var*)(this + 1);
    }

    template<class Fn>
    void MapArgs(Fn callback)
    {
        Js::Var* arguments = this->GetArguments();
        for (uint i = 0; i < callInfo.Count; i++)
        {
            callback(i, (Js::Var*)((uintptr_t*)arguments + i));
        }
    }

    InlinedFrameLayout* Next()
    {
        InlinedFrameLayout *next = (InlinedFrameLayout *)(this->GetArguments() + this->callInfo.Count);
        return next;
    }
};

class BailOutRecord;

struct LazyBailOutRecord
{
    uint32 offset;
    BYTE* instructionPointer; // Instruction pointer of the bailout code
    BailOutRecord* bailoutRecord;

    LazyBailOutRecord() : offset(0), instructionPointer(nullptr), bailoutRecord(nullptr) {}

    LazyBailOutRecord(uint32 offset, BYTE* address, BailOutRecord* record) :
        offset(offset), instructionPointer(address),
        bailoutRecord(record)
    {}

    void SetBailOutKind();
#if DBG
    void Dump(Js::FunctionBody* functionBody);
#endif
};

struct StackFrameConstants
{
#if defined(_M_IX86)
    static const size_t StackCheckCodeHeightThreadBound = 35;
    static const size_t StackCheckCodeHeightNotThreadBound = 47;
    static const size_t StackCheckCodeHeightWithInterruptProbe = 53;
#elif defined(_M_X64)
    static const size_t StackCheckCodeHeightThreadBound = 57;
    static const size_t StackCheckCodeHeightNotThreadBound = 62;
    static const size_t StackCheckCodeHeightWithInterruptProbe = 68;
#elif defined(_M_ARM)
    static const size_t StackCheckCodeHeight = 64;
    static const size_t StackCheckCodeHeightThreadBound = StackFrameConstants::StackCheckCodeHeight;
    static const size_t StackCheckCodeHeightNotThreadBound = StackFrameConstants::StackCheckCodeHeight;
    static const size_t StackCheckCodeHeightWithInterruptProbe = StackFrameConstants::StackCheckCodeHeight;
#elif defined(_M_ARM64)
    static const size_t StackCheckCodeHeight = 58*2;
    static const size_t StackCheckCodeHeightThreadBound = StackFrameConstants::StackCheckCodeHeight;
    static const size_t StackCheckCodeHeightNotThreadBound = StackFrameConstants::StackCheckCodeHeight;
    static const size_t StackCheckCodeHeightWithInterruptProbe = StackFrameConstants::StackCheckCodeHeight;
#endif
};

struct NativeResourceIds
{
    static const short SourceCodeResourceNameId = 0x64;
    static const short ByteCodeResourceNameId = 0x65;
    static const short NativeMapResourceNameId = 0x66;
    static const short NativeThrowMapResourceNameId = 0x67;
};

#if defined(_M_IX86)
struct ThunkConstants
{
    static const size_t ThunkInstructionSize = 2;
    static const size_t ThunkSize = 6;
};
#endif

enum LibraryValue {
    ValueInvalid,
    ValueUndeclBlockVar,
    ValueEmptyString,
    ValueUndefined,
    ValueNull,
    ValueTrue,
    ValueFalse,
    ValueNegativeZero,
    ValueNumberTypeStatic,
    ValueStringTypeStatic,
    ValueObjectType,
    ValueObjectHeaderInlinedType,
    ValueRegexType,
    ValueArrayConstructor,
    ValuePositiveInfinity,
    ValueNaN,
    ValueJavascriptArrayType,
    ValueNativeIntArrayType,
    ValueNativeFloatArrayType,
    ValueConstructorCacheDefaultInstance,
    ValueAbsDoubleCst,
    ValueUintConvertConst,
    ValueBuiltinFunctions,
    ValueJnHelperMethods,
    ValueCharStringCache
};

enum VTableValue {
#if !_M_X64
    VtableJavascriptNumber,
#endif
    VtableDynamicObject,
    VtableInvalid,
    VtablePropertyString,
    VtableJavascriptBoolean,
    VtableSmallDynamicObjectSnapshotEnumeratorWPCache,
    VtableJavascriptArray,
    VtableInt8Array,
    VtableUint8Array,
    VtableUint8ClampedArray,
    VtableInt16Array,
    VtableUint16Array,
    VtableInt32Array,
    VtableUint32Array,
    VtableFloat32Array,
    VtableFloat64Array,
    VtableJavascriptPixelArray,
    VtableInt64Array,
    VtableUint64Array,
    VtableBoolArray,
    VtableCharArray,
    VtableInt8VirtualArray,
    VtableUint8VirtualArray,
    VtableUint8ClampedVirtualArray,
    VtableInt16VirtualArray,
    VtableUint16VirtualArray,
    VtableInt32VirtualArray,
    VtableUint32VirtualArray,
    VtableFloat32VirtualArray,
    VtableFloat64VirtualArray,
    VtableNativeIntArray,
#if ENABLE_COPYONACCESS_ARRAY
    VtableCopyOnAccessNativeIntArray,
#endif
    VtableNativeFloatArray,
    VtableJavascriptNativeIntArray,
    VtableJavascriptRegExp,
    VtableStackScriptFunction,
    VtableConcatStringMulti,
    VtableCompoundString,
    // SIMD_JS
    VtableSimd128F4,
    VtableSimd128I4,
    Count
};

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
const wchar_t *GetVtableName(VTableValue value);
#endif

enum AuxArrayValue {
    AuxPropertyIdArray,
    AuxIntArray,
    AuxFloatArray,
    AuxVarsArray,
    AuxVarArrayVarCount,
    AuxFuncInfoArray
};

enum OptimizationOverridesValue {
    OptimizationOverridesArraySetElementFastPathVtable,
    OptimizationOverridesIntArraySetElementFastPathVtable,
    OptimizationOverridesFloatArraySetElementFastPathVtable,
    OptimizationOverridesSideEffects
};

enum FunctionBodyValue {
    FunctionBodyConstantVar,
    FunctionBodyNestedFuncReference,
    FunctionBodyReferencedPropertyId,
    FunctionBodyPropertyIdFromCacheId,
    FunctionBodyLiteralRegex,
    FunctionBodyStringTemplateCallsiteRef
};

enum ScriptContextValue {
    ScriptContextNumberAllocator,
    ScriptContextCharStringCache,
    ScriptContextRecycler,
    ScriptContextOptimizationOverrides
};

enum NumberAllocatorValue {
    NumberAllocatorEndAddress,
    NumberAllocatorFreeObjectList
};
