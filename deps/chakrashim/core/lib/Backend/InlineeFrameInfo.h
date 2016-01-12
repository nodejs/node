//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct BailoutConstantValue {
public:
    void InitIntConstValue(int32 value) { this->type = TyInt32; this->u.intConst.value = (IntConstType)value; };
    void InitIntConstValue(IntConstType value, IRType type) {
        Assert(IRType_IsSignedInt(type));
        this->type = type; this->u.intConst.value = value;
    };
    void InitVarConstValue(Js::Var value);
    void InitFloatConstValue(FloatConstType value) { this->type = TyFloat64; this->u.floatConst.value = value; }
public:
    IRType type;
    union
    {
        struct sIntConst
        {
            IntConstType value;
        } intConst;
        struct sVarConst
        {
            Js::Var value;
        } varConst;
        struct sFloatConst
        {
            FloatConstType value;
        } floatConst;
    } u;
    Js::Var ToVar(Func* func, Js::ScriptContext* scriptContext) const;
};

enum InlineeFrameInfoValueType
{
    InlineeFrameInfoValueType_None,
    InlineeFrameInfoValueType_Sym,
    InlineeFrameInfoValueType_Const
};

struct InlineFrameInfoValue
{
    InlineeFrameInfoValueType type;
    union
    {
        StackSym* sym;
        BailoutConstantValue constValue;
    };

    bool IsConst() { return this->type == InlineeFrameInfoValueType_Const; }

    InlineFrameInfoValue() : type(InlineeFrameInfoValueType_None), sym(nullptr) {}
    InlineFrameInfoValue(StackSym* sym) : type(InlineeFrameInfoValueType_Sym), sym(sym) {}
    InlineFrameInfoValue(BailoutConstantValue value) : type(InlineeFrameInfoValueType_Const), constValue(value) {}

};
struct InlineeFrameInfo;

struct InlineeFrameRecord
{
    int functionOffset;
    int inlineDepth;
    uint inlineeStartOffset;
    int* argOffsets;
    Js::Var * constants;
    InlineeFrameRecord* parent;

    uint argCount;
    BVUnit floatArgs;
    BVUnit losslessInt32Args;

    template<class Fnc>
    void MapOffsets(Fnc callback)
    {
        callback(functionOffset);

        for (uint i = 0; i < argCount; i++)
        {
            callback(argOffsets[i]);
        }
    }

#if DBG_DUMP
    uint constantCount;
    Js::FunctionBody* functionBody;
    InlineeFrameInfo* frameInfo;
#endif

    // Fields are zero initialized any way
    InlineeFrameRecord(uint argCount, Js::FunctionBody* functionBody, InlineeFrameInfo* frameInfo) : argCount(argCount)
#if DBG_DUMP
        , functionBody(functionBody)
        , frameInfo(frameInfo)
#endif
    {}

    static InlineeFrameRecord* New(NativeCodeData::Allocator* alloc, uint argCount, uint constantCount, Js::FunctionBody* functionBody, InlineeFrameInfo* frameInfo)
    {
        InlineeFrameRecord* record = NativeCodeDataNewZ(alloc, InlineeFrameRecord, argCount, functionBody, frameInfo);
        record->argOffsets = NativeCodeDataNewArray(alloc, int, argCount);
        record->constants = NativeCodeDataNewArray(alloc, Js::Var, constantCount);
        DebugOnly(record->constantCount = constantCount);
        return record;
    }

    void PopulateParent(Func* func);
    void RestoreFrames(Js::FunctionBody* functionBody, InlinedFrameLayout* outerMostInlinee, Js::JavascriptCallStackLayout* callstack);
    void Finalize(Func* inlinee, uint currentOffset);
#if DBG_DUMP
    void Dump() const;
    void DumpOffset(int offset) const;
#endif

private:
    void Restore(Js::FunctionBody* functionBody, InlinedFrameLayout *outerMostFrame, Js::JavascriptCallStackLayout * layout) const;
    Js::Var Restore(int offset, bool isFloat64, bool isInt32, Js::JavascriptCallStackLayout * layout, Js::FunctionBody* functionBody) const;
    InlineeFrameRecord* Reverse();
};

struct NativeOffsetInlineeFramePair
{
    uint32 offset;
    InlineeFrameRecord* record;
};

struct InlineeFrameInfo
{
    typedef JsUtil::List<InlineFrameInfoValue, JitArenaAllocator, /*isLeaf*/ false> ArgList;
    InlineFrameInfoValue function;
    ArgList* arguments;
    InlineeFrameRecord* record;
    BVSparse<JitArenaAllocator>* floatSyms;
    BVSparse<JitArenaAllocator>* intSyms;

    BVSparse<JitArenaAllocator>* simd128F4Syms;
    BVSparse<JitArenaAllocator>* simd128I4Syms;
    bool isRecorded;

    static InlineeFrameInfo* New(JitArenaAllocator* alloc)
    {
        InlineeFrameInfo* frameInfo = JitAnewStructZ(alloc, InlineeFrameInfo);
        frameInfo->arguments = JitAnew(alloc, ArgList, alloc);
        return frameInfo;
    }

    template<class Fn>
    void IterateSyms(Fn callback, bool inReverse = false)
    {
        auto iterator = [=](uint index, InlineFrameInfoValue& value)
        {
            if (value.type == InlineeFrameInfoValueType_Sym)
            {
                callback(value.sym);
            }
            Assert(value.type != InlineeFrameInfoValueType_None);
        };

        if (inReverse && function.type == InlineeFrameInfoValueType_Sym)
        {
            callback(function.sym);
        }

        if (inReverse)
        {
            arguments->ReverseMap(iterator);
        }
        else
        {
            arguments->Map(iterator);
        }
        Assert(function.type != InlineeFrameInfoValueType_None);
        if (!inReverse && function.type == InlineeFrameInfoValueType_Sym)
        {
            callback(function.sym);
        }
    }
    void AllocateRecord(Func* func, Js::FunctionBody* functionBody);

#if DBG_DUMP
    void Dump() const;
#endif

};


