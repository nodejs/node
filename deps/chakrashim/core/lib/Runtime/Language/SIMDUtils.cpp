//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

namespace Js
{

    int32 SIMDCheckTypedArrayIndex(ScriptContext* scriptContext, Var index)
    {
        int32 int32Value;
        Assert(index != NULL);

        int32Value = SIMDCheckInt32Number(scriptContext, index);
        return int32Value;
    }

    int32 SIMDCheckLaneIndex(ScriptContext* scriptContext, Var lane, const int32 range)
    {
        int32 int32Value;
        Assert(lane != NULL);

        int32Value = SIMDCheckInt32Number(scriptContext, lane);

        if (int32Value < 0 || int32Value >= range)
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_SimdLaneRangeError);
        }

        return int32Value;
    }

    // Is Number with int32 value.
    int32 SIMDCheckInt32Number(ScriptContext* scriptContext, Var value)
    {
        int32 int32Value;

        if (JavascriptNumber::Is(value))
        {
            if (!JavascriptNumber::TryGetInt32Value(JavascriptNumber::GetValue(value), &int32Value))
            {
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange);
            }
        }
        else if (TaggedInt::Is(value))
        {
            int32Value = TaggedInt::ToInt32(value);
        }
        else
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedNumber, L"index");
        }
        return int32Value;
    }

    template <int laneCount> SIMDValue SIMD128InnerShuffle(SIMDValue src1, SIMDValue src2, int32 lane0, int32 lane1, int32 lane2, int32 lane3)
    {
        SIMDValue result;
        CompileAssert(laneCount == 4 || laneCount == 2);
        if (laneCount == 4)
        {
            result.i32[SIMD_X] = lane0 < 4 ? src1.i32[lane0] : src2.i32[lane0 - 4];
            result.i32[SIMD_Y] = lane1 < 4 ? src1.i32[lane1] : src2.i32[lane1 - 4];
            result.i32[SIMD_Z] = lane2 < 4 ? src1.i32[lane2] : src2.i32[lane2 - 4];
            result.i32[SIMD_W] = lane3 < 4 ? src1.i32[lane3] : src2.i32[lane3 - 4];
        }
        else
        {
            result.f64[SIMD_X] = lane0 < 2 ? src1.f64[lane0] : src2.f64[lane0 - 2];
            result.f64[SIMD_Y] = lane1 < 2 ? src1.f64[lane1] : src2.f64[lane1 - 2];
        }
        return result;
    }

    template <class SIMDType, int laneCount> Var SIMD128SlowShuffle(Var src1, Var src2, Var lane0, Var lane1, Var lane2, Var lane3, int range, ScriptContext* scriptContext)
    {
        SIMDType *a = SIMDType::FromVar(src1);
        SIMDType *b = SIMDType::FromVar(src2);
        Assert(a);
        Assert(b);

        int32 lane0Value = 0;
        int32 lane1Value = 0;
        int32 lane2Value = 0;
        int32 lane3Value = 0;

        SIMDValue src1Value = a->GetValue();
        SIMDValue src2Value = b->GetValue();
        SIMDValue result;

        CompileAssert(laneCount == 4 || laneCount == 2);

        if (laneCount == 4)
        {
            lane0Value = SIMDCheckLaneIndex(scriptContext, lane0, range);
            lane1Value = SIMDCheckLaneIndex(scriptContext, lane1, range);
            lane2Value = SIMDCheckLaneIndex(scriptContext, lane2, range);
            lane3Value = SIMDCheckLaneIndex(scriptContext, lane3, range);

            Assert(lane0Value >= 0 && lane0Value < range);
            Assert(lane1Value >= 0 && lane1Value < range);
            Assert(lane2Value >= 0 && lane2Value < range);
            Assert(lane3Value >= 0 && lane3Value < range);

            result = SIMD128InnerShuffle(src1Value, src2Value, lane0Value, lane1Value, lane2Value, lane3Value);
        }
        else
        {
            lane0Value = SIMDCheckLaneIndex(scriptContext, lane0, range);
            lane1Value = SIMDCheckLaneIndex(scriptContext, lane1, range);

            Assert(lane0Value >= 0 && lane0Value < range);
            Assert(lane1Value >= 0 && lane1Value < range);

            result = SIMD128InnerShuffle<2>(src1Value, src2Value, lane0Value, lane1Value, lane2Value, lane3Value);
        }

        return SIMDType::New(&result, scriptContext);
    }

    template Var SIMD128SlowShuffle<JavascriptSIMDInt32x4, 4>   (Var src1, Var src2, Var lane0, Var lane1, Var lane2, Var lane3, int range, ScriptContext* scriptContext);
    template Var SIMD128SlowShuffle<JavascriptSIMDFloat32x4, 4> (Var src1, Var src2, Var lane0, Var lane1, Var lane2, Var lane3, int range, ScriptContext* scriptContext);
    template Var SIMD128SlowShuffle<JavascriptSIMDFloat64x2, 2> (Var src1, Var src2, Var lane0, Var lane1, Var lane2, Var lane3, int range, ScriptContext* scriptContext);

    //Int8x16 LaneAccess
    inline int8 SIMD128InnerExtractLaneI16(const SIMDValue& src1, const int32 lane)
    {
        return src1.i8[lane];
    }

    inline SIMDValue SIMD128InnerReplaceLaneI16(const SIMDValue& src1, const int32 lane, const int8 value)
    {
        SIMDValue result = src1;
        result.i8[lane] = value;
        return result;
    }

    static inline int8 SIMD128GetLaneValue(JavascriptSIMDInt8x16 *jsVal, const int laneValue)
    {
        Assert(jsVal);
        return SIMD128InnerExtractLaneI16(jsVal->GetValue(), laneValue);
    }

    static inline SIMDValue SIMD128SetLaneValue(JavascriptSIMDInt8x16 *jsVal, const int laneValue, int8 value)
    {
        Assert(jsVal);
        return SIMD128InnerReplaceLaneI16(jsVal->GetValue(), laneValue, value);
    }

    //Int32x4 LaneAccess
    inline int SIMD128InnerExtractLaneI4(const SIMDValue& src1, const int32 lane)
    {
        return src1.i32[lane];
    }

    inline SIMDValue SIMD128InnerReplaceLaneI4(const SIMDValue& src1, const int32 lane, const int value)
    {
        SIMDValue result = src1;
        result.i32[lane] = value;
        return result;
    }

    static inline int SIMD128GetLaneValue(JavascriptSIMDInt32x4 *jsVal, const int laneValue)
    {
        Assert(jsVal);
        return SIMD128InnerExtractLaneI4(jsVal->GetValue(), laneValue);
    }

    static inline SIMDValue SIMD128SetLaneValue(JavascriptSIMDInt32x4 *jsVal, const int laneValue, int value)
    {
        Assert(jsVal);
        return SIMD128InnerReplaceLaneI4(jsVal->GetValue(), laneValue, value);
    }

    //Float32x4 LaneAccess
    inline float SIMD128InnerExtractLaneF4(const SIMDValue& src1, const int32 lane)
    {
        return src1.f32[lane];
    }

    inline SIMDValue SIMD128InnerReplaceLaneF4(const SIMDValue& src1, const int32 lane, const float value)
    {
        SIMDValue result = src1;
        result.f32[lane] = value;
        return result;
    }

    static inline float SIMD128GetLaneValue(JavascriptSIMDFloat32x4 *jsVal, const int laneValue)
    {
        Assert(jsVal);
        return SIMD128InnerExtractLaneF4(jsVal->GetValue(), laneValue);
    }

    static inline SIMDValue SIMD128SetLaneValue(JavascriptSIMDFloat32x4 *jsVal, const int laneValue, float value)
    {
        Assert(jsVal);
        return SIMD128InnerReplaceLaneF4(jsVal->GetValue(), laneValue, value);
    }

    template<class SIMDType, int laneCount, typename T>
    inline T SIMD128ExtractLane(const Var src, const Var lane, ScriptContext* scriptContext)
    {
        SIMDType *jsVal = SIMDType::FromVar(src);
        Assert(jsVal);

        int32 laneValue = SIMDCheckLaneIndex(scriptContext, lane, laneCount);
        Assert(laneValue >= 0 && laneValue < laneCount);

        return SIMD128GetLaneValue(jsVal, laneValue);
    }

    template<class SIMDType, int laneCount, typename T>
    inline SIMDValue SIMD128ReplaceLane(const Var src, const Var lane, const T value, ScriptContext* scriptContext)
    {
        SIMDType *jsVal = SIMDType::FromVar(src);
        Assert(jsVal);

        int32 laneValue = SIMDCheckLaneIndex(scriptContext, lane, laneCount);
        Assert(laneValue >= 0 && laneValue < laneCount);

        return SIMD128SetLaneValue(jsVal, laneValue, value);
    }

    template int8  SIMD128ExtractLane<JavascriptSIMDInt8x16, 16, int8>(Var src, Var lane, ScriptContext* scriptContext);
    template SIMDValue SIMD128ReplaceLane<JavascriptSIMDInt8x16, 16, int8>(Var src, Var lane, int8 value, ScriptContext* scriptContext);
    template int   SIMD128ExtractLane<JavascriptSIMDInt32x4, 4, int>(Var src, Var lane, ScriptContext* scriptContext);
    template SIMDValue SIMD128ReplaceLane<JavascriptSIMDInt32x4, 4, int>(Var src, Var lane, int value, ScriptContext* scriptContext);
    template float SIMD128ExtractLane<JavascriptSIMDFloat32x4, 4, float>(Var src, Var lane, ScriptContext* scriptContext);
    template SIMDValue SIMD128ReplaceLane<JavascriptSIMDFloat32x4, 4, float>(Var src, Var lane, float value, ScriptContext* scriptContext);

    bool SIMDIsSupportedTypedArray(Var value)
    {
        return JavascriptOperators::GetTypeId(value) >= TypeIds_Int8Array && JavascriptOperators::GetTypeId(value) <= TypeIds_Float64Array;
    }

    /*
    Checks if:
    1. Array is supported typed array
    2. Lane index is a Number/TaggedInt and int32 value
    3. Lane index is within array bounds
    */

    SIMDValue* SIMDCheckTypedArrayAccess(Var arg1, Var arg2, TypedArrayBase **tarray, int32 *index, uint32 dataWidth, ScriptContext *scriptContext)
    {
        if (!SIMDIsSupportedTypedArray(arg1))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInvalidArgType, L"Simd typed array access");
        }
        *index = SIMDCheckInt32Number(scriptContext, arg2);

        // bound check
        *tarray = TypedArrayBase::FromVar(arg1);
        uint32 bpe = (*tarray)->GetBytesPerElement();
        int32 offset = (*index) * bpe;
        if (offset < 0 || (offset + dataWidth) >(int32)(*tarray)->GetByteLength())
        {
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, L"Simd typed array access");
        }
        return (SIMDValue*)((*tarray)->GetByteBuffer() + offset);
    }

    SIMDValue SIMDLdData(SIMDValue *data, uint8 dataWidth)
    {
        SIMDValue result = { 0, 0, 0, 0 };
        // bitwise copy. Always use integer fields to avoid wrong copy of NaNs.
        switch (dataWidth)
        {
        case 16:
            result.i32[SIMD_W] = data->i32[SIMD_W];
            // fall through
        case 12:
            result.i32[SIMD_Z] = data->i32[SIMD_Z];
            // fall through
        case 8:
            result.i32[SIMD_Y] = data->i32[SIMD_Y];
            // fall through
        case 4:
            result.i32[SIMD_X] = data->i32[SIMD_X];
            break;
        default:
            Assert(UNREACHED);
        }
        return result;
    }

    void SIMDStData(SIMDValue *data, SIMDValue simdValue, uint8 dataWidth)
    {
        // bitwise copy. Always use integer fields to avoid wrong copy of NaNs.
        switch (dataWidth)
        {
        case 16:
            data->i32[SIMD_W] = simdValue.i32[SIMD_W];
            // fall through
        case 12:
            data->i32[SIMD_Z] = simdValue.i32[SIMD_Z];
            // fall through
        case 8:
            data->i32[SIMD_Y] = simdValue.i32[SIMD_Y];
            // fall through
        case 4:
            data->i32[SIMD_X] = simdValue.i32[SIMD_X];
            break;
        default:
            Assert(UNREACHED);
        }
    }

    template <class SIMDType>
    Var SIMD128TypedArrayLoad(Var arg1, Var arg2, uint32 dataWidth, ScriptContext *scriptContext)
    {
        Assert(dataWidth >= 4 && dataWidth <= 16);

        TypedArrayBase *tarray = NULL;
        int32 index = -1;
        SIMDValue* data = NULL;

        data = SIMDCheckTypedArrayAccess(arg1, arg2, &tarray, &index, dataWidth, scriptContext);

        Assert(tarray != NULL);
        Assert(index >= 0);
        Assert(data != NULL);

        SIMDValue result = SIMDLdData(data, (uint8)dataWidth);

        return SIMDType::New(&result, scriptContext);
    }

    template Var SIMD128TypedArrayLoad<JavascriptSIMDFloat32x4>(Var arg1, Var arg2, uint32 dataWidth, ScriptContext *scriptContext);
    template Var SIMD128TypedArrayLoad<JavascriptSIMDInt32x4>(Var arg1, Var arg2, uint32 dataWidth, ScriptContext *scriptContext);
    template Var SIMD128TypedArrayLoad<JavascriptSIMDFloat64x2>(Var arg1, Var arg2, uint32 dataWidth, ScriptContext *scriptContext);

    template <class SIMDType>
    void SIMD128TypedArrayStore(Var arg1, Var arg2, Var simdVar, uint32 dataWidth, ScriptContext *scriptContext)
    {
        Assert(dataWidth >= 4 && dataWidth <= 16);

        TypedArrayBase *tarray = NULL;
        int32 index = -1;
        SIMDValue* data = NULL;

        data = SIMDCheckTypedArrayAccess(arg1, arg2, &tarray, &index, dataWidth, scriptContext);

        Assert(tarray != NULL);
        Assert(index >= 0);
        Assert(data != NULL);

        SIMDValue simdValue = SIMDType::FromVar(simdVar)->GetValue();
        SIMDStData(data, simdValue, (uint8)dataWidth);
    }

    template void SIMD128TypedArrayStore<JavascriptSIMDFloat32x4>(Var arg1, Var arg2, Var simdVar, uint32 dataWidth, ScriptContext *scriptContext);
    template void SIMD128TypedArrayStore<JavascriptSIMDInt32x4>(Var arg1, Var arg2, Var simdVar, uint32 dataWidth, ScriptContext *scriptContext);
    template void SIMD128TypedArrayStore<JavascriptSIMDFloat64x2>(Var arg1, Var arg2, Var simdVar, uint32 dataWidth, ScriptContext *scriptContext);

#if ENABLE_NATIVE_CODEGEN
    // Maps Simd opcodes which are non-contigous to a zero-based linear space. Used to index a table using an Simd opcode.
    uint32 SimdOpcodeAsIndex(Js::OpCode op)
    {
        if (op <= Js::OpCode::Simd128_End)
        {
            return (uint32)((Js::OpCode)op - Js::OpCode::Simd128_Start);
        }
        else
        {
            Assert(op >= Js::OpCode::Simd128_Start_Extend && op <= Js::OpCode::Simd128_End_Extend);
            return (uint32)((Js::OpCode)op - Js::OpCode::Simd128_Start_Extend) + (uint32)(Js::OpCode::Simd128_End - Js::OpCode::Simd128_Start) + 1;
        }
    }
#endif
}
