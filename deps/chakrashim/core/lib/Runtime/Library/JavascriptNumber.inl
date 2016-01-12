//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Js
{
#if FLOATVAR
    __inline JavascriptNumber::JavascriptNumber(double value, StaticType*)
    {
        AssertMsg(!IsNan(value) || ToSpecial(value) == k_Nan || ToSpecial(value) == 0x7FF8000000000000ull, "We should only produce a NaN with this value");
        SetSpecial(ToSpecial(value) ^ FloatTag_Value);
    }
#else
    __inline JavascriptNumber::JavascriptNumber(double value, StaticType * type) : RecyclableObject(type), m_value(value)
    {
        Assert(type->GetTypeId() == TypeIds_Number);
    }
#endif

    __forceinline Var JavascriptNumber::ToVar(int32 nValue, ScriptContext* scriptContext)
    {
        if (!TaggedInt::IsOverflow(nValue))
        {
            return TaggedInt::ToVarUnchecked(nValue);
        }
        else
        {
            return JavascriptNumber::NewInlined((double) nValue, scriptContext);
        }
    }

    __inline Var JavascriptNumber::ToVar(uint32 nValue, ScriptContext* scriptContext)
    {
        return !TaggedInt::IsOverflow(nValue) ? TaggedInt::ToVarUnchecked(nValue) :
            JavascriptNumber::New((double) nValue,scriptContext);
    }

    __inline Var JavascriptNumber::ToVar(int64 nValue, ScriptContext* scriptContext)
    {
        return !TaggedInt::IsOverflow(nValue) ?
                TaggedInt::ToVarUnchecked((int) nValue) :
                JavascriptNumber::New((double) nValue,scriptContext);
    }

    __inline Var JavascriptNumber::ToVar(uint64 nValue, ScriptContext* scriptContext)
    {
        return !TaggedInt::IsOverflow(nValue) ?
                TaggedInt::ToVarUnchecked((int) nValue) :
                JavascriptNumber::New((double) nValue,scriptContext);
    }

    inline bool JavascriptNumber::TryToVarFast(int32 nValue, Var* result)
    {
        if (!TaggedInt::IsOverflow(nValue))
        {
            *result = TaggedInt::ToVarUnchecked(nValue);
            return true;
        }

#if FLOATVAR
        *result = JavascriptNumber::ToVar((double)nValue);
        return true;
#else
        return false;
#endif
    }

    inline bool JavascriptNumber::TryToVarFastWithCheck(double value, Var* result)
    {
#if FLOATVAR
        if (IsNan(value))
        {
            value = JavascriptNumber::NaN;
        }

        *result = JavascriptNumber::ToVar(value);
        return true;
#else
        return false;
#endif
    }

#if FLOATVAR
    inline bool JavascriptNumber::Is(Var aValue)
    {
        return Is_NoTaggedIntCheck(aValue);
    }

    __inline JavascriptNumber* JavascriptNumber::InPlaceNew(double value, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {
        AssertMsg( result != NULL, "Cannot use InPlaceNew without a value result location" );
        result = (JavascriptNumber*)ToVar(value);
        return result;
    }

    inline Var JavascriptNumber::New(double value, ScriptContext* scriptContext)
    {
        return ToVar(value);
    }

    inline Var JavascriptNumber::NewWithCheck(double value, ScriptContext* scriptContext)
    {
        if (IsNan(value))
        {
            value = JavascriptNumber::NaN;
        }
        return ToVar(value);
    }

    __inline Var JavascriptNumber::NewInlined(double value, ScriptContext* scriptContext)
    {
        return ToVar(value);
    }

#if ENABLE_NATIVE_CODEGEN
    __inline Var JavascriptNumber::NewCodeGenInstance(CodeGenNumberAllocator *alloc, double value, ScriptContext* scriptContext)
    {
        return ToVar(value);
    }
#endif

    __inline Var JavascriptNumber::ToVar(double value)
    {
        uint64 val = *(uint64*)&value;
        AssertMsg(!IsNan(value) || ToSpecial(value) == k_Nan || ToSpecial(value) == 0x7FF8000000000000ull, "We should only produce a NaN with this value");
        return reinterpret_cast<Var>(val ^ FloatTag_Value);
    }

#else
    inline bool JavascriptNumber::Is(Var aValue)
    {
        return !TaggedInt::Is(aValue) && Is_NoTaggedIntCheck(aValue);
    }

#if !defined(USED_IN_STATIC_LIB)
    inline bool JavascriptNumber::Is_NoTaggedIntCheck(Var aValue)
    {
        RecyclableObject* object = RecyclableObject::FromVar(aValue);
        AssertMsg((object->GetTypeId() == TypeIds_Number) == VirtualTableInfo<JavascriptNumber>::HasVirtualTable(object), "JavascriptNumber has no unique VTABLE?");
        return VirtualTableInfo<JavascriptNumber>::HasVirtualTable(object);
    }
#endif

    inline JavascriptNumber* JavascriptNumber::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNumber'");

        return reinterpret_cast<JavascriptNumber *>(aValue);
    }

    __inline double JavascriptNumber::GetValue(Var aValue)
     {
         AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNumber'");

         return JavascriptNumber::FromVar(aValue)->GetValue();
     }

    __inline JavascriptNumber* JavascriptNumber::InPlaceNew(double value, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {
        AssertMsg( result != NULL, "Cannot use InPlaceNew without a value result location" );
        Assume(result != NULL); // Encourage the compiler to omit a NULL check on the return from placement new
        return ::new(result) JavascriptNumber(value, scriptContext->GetLibrary()->GetNumberTypeStatic());
    }

    inline Var JavascriptNumber::New(double value, ScriptContext* scriptContext)
    {
        return scriptContext->GetLibrary()->CreateNumber(value, scriptContext->GetNumberAllocator());
    }

    inline Var JavascriptNumber::NewWithCheck(double value, ScriptContext* scriptContext)
    {
        return scriptContext->GetLibrary()->CreateNumber(value, scriptContext->GetNumberAllocator());
    }

    __inline Var JavascriptNumber::NewInlined(double value, ScriptContext* scriptContext)
    {
        return scriptContext->GetLibrary()->CreateNumber(value, scriptContext->GetNumberAllocator());
    }

#if ENABLE_NATIVE_CODEGEN
    __inline Var JavascriptNumber::NewCodeGenInstance(CodeGenNumberAllocator *alloc, double value, ScriptContext* scriptContext)
    {
        return scriptContext->GetLibrary()->CreateCodeGenNumber(alloc, value);
    }
#endif

#endif

    __inline JavascriptString * JavascriptNumber::ToStringNan(ScriptContext* scriptContext)
    {
        return ToStringNan(*scriptContext->GetLibrary());
    }

    __inline JavascriptString* JavascriptNumber::ToStringNanOrInfinite(double value, ScriptContext* scriptContext)
    {
        return ToStringNanOrInfinite(value, *scriptContext->GetLibrary());
    }

    __inline Var JavascriptNumber::FormatDoubleToString( double value, Js::NumberUtilities::FormatType formatType, int formatDigits, ScriptContext* scriptContext )
    {
        static const int bufSize = 256;
        wchar_t szBuffer[bufSize] = L"";
        wchar_t * psz = szBuffer;
        wchar_t * pszToBeFreed = NULL;
        int nOut;

        if ((nOut = Js::NumberUtilities::FDblToStr(value, formatType, formatDigits, szBuffer, bufSize)) > bufSize )
        {
            int nOut1;
            pszToBeFreed = psz = (wchar_t *)malloc(nOut * sizeof(wchar_t));
            if(0 == psz)
            {
                Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }

            nOut1 = Js::NumberUtilities::FDblToStr(value, Js::NumberUtilities::FormatFixed, formatDigits, psz, nOut);
            Assert(nOut1 == nOut);
        }

        // nOut includes room for terminating NUL
        JavascriptString* result = JavascriptString::NewCopyBuffer(psz, nOut - 1, scriptContext);

        if(pszToBeFreed)
        {
            free(pszToBeFreed);
        }

        return result;
    }
}
