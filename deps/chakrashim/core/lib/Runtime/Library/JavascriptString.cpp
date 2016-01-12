//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "DataStructures\BigInt.h"
#include "Library\EngineInterfaceObject.h"
#include "Library\IntlEngineInterfaceExtensionObject.h"

namespace Js
{
    // White Space characters are defined in ES6 Section 11.2
    // There are 26 white space characters we need to correctly class:
    //0x0009
    //0x000a
    //0x000b
    //0x000c
    //0x000d
    //0x0020
    //0x00a0
    //0x1680
    //0x180e
    //0x2000
    //0x2001
    //0x2002
    //0x2003
    //0x2004
    //0x2005
    //0x2006
    //0x2007
    //0x2008
    //0x2009
    //0x200a
    //0x2028
    //0x2029
    //0x202f
    //0x205f
    //0x3000
    //0xfeff
    bool IsWhiteSpaceCharacter(wchar_t ch)
    {
        return ch >= 0x9 &&
            (ch <= 0xd ||
                (ch <= 0x200a &&
                    (ch >= 0x2000 || ch == 0x20 || ch == 0xa0 || ch == 0x1680 || ch == 0x180e)
                ) ||
                (ch >= 0x2028 &&
                    (ch <= 0x2029 || ch == 0x202f || ch == 0x205f || ch == 0x3000 || ch == 0xfeff)
                )
            );
    }

    template <typename T, bool copyBuffer>
    JavascriptString* JavascriptString::NewWithBufferT(const wchar_t * content, charcount_t cchUseLength, ScriptContext * scriptContext)
    {
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");
        AssertMsg(IsValidCharCount(cchUseLength), "String length will overflow an int");
        switch (cchUseLength)
        {
        case 0:
            return scriptContext->GetLibrary()->GetEmptyString();

        case 1:
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(*content);

        default:
            break;
        }

        Recycler* recycler = scriptContext->GetRecycler();
        StaticType * stringTypeStatic = scriptContext->GetLibrary()->GetStringTypeStatic();
        wchar_t const * buffer = content;

        charcount_t cchUseBoundLength = static_cast<charcount_t>(cchUseLength);
        if (copyBuffer)
        {
             buffer = JavascriptString::AllocateLeafAndCopySz(recycler, content, cchUseBoundLength);
        }

        return T::New(stringTypeStatic, buffer, cchUseBoundLength, recycler);
    }

    JavascriptString* JavascriptString::NewWithSz(__in_z const wchar_t * content, ScriptContext * scriptContext)
    {
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");
        return NewWithBuffer(content, GetBufferLength(content), scriptContext);
    }

    JavascriptString* JavascriptString::NewWithArenaSz(__in_z const wchar_t * content, ScriptContext * scriptContext)
    {
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");
        return NewWithArenaBuffer(content, GetBufferLength(content), scriptContext);
    }

    JavascriptString* JavascriptString::NewWithBuffer(__in_ecount(cchUseLength) const wchar_t * content, charcount_t cchUseLength, ScriptContext * scriptContext)
    {
        return NewWithBufferT<LiteralString, false>(content, cchUseLength, scriptContext);
    }

    JavascriptString* JavascriptString::NewWithArenaBuffer(__in_ecount(cchUseLength) const wchar_t* content, charcount_t cchUseLength, ScriptContext* scriptContext)
    {
        return NewWithBufferT<ArenaLiteralString, false>(content, cchUseLength, scriptContext);
    }

    JavascriptString* JavascriptString::NewCopySz(__in_z const wchar_t* content, ScriptContext* scriptContext)
    {
        return NewCopyBuffer(content, GetBufferLength(content), scriptContext);
    }

    JavascriptString* JavascriptString::NewCopyBuffer(__in_ecount(cchUseLength) const wchar_t* content, charcount_t cchUseLength, ScriptContext* scriptContext)
    {
        return NewWithBufferT<LiteralString, true>(content, cchUseLength, scriptContext);
    }

    JavascriptString* JavascriptString::NewCopySzFromArena(__in_z const wchar_t* content, ScriptContext* scriptContext, ArenaAllocator *arena)
    {
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");

        charcount_t cchUseLength = JavascriptString::GetBufferLength(content);
        wchar_t* buffer = JavascriptString::AllocateAndCopySz(arena, content, cchUseLength);
        return ArenaLiteralString::New(scriptContext->GetLibrary()->GetStringTypeStatic(),
            buffer, cchUseLength, arena);
    }

    Var JavascriptString::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Negative argument count");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && RecyclableObject::Is(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        JavascriptString* str;
        Var result;

        if (args.Info.Count > 1)
        {
            if (JavascriptSymbol::Is(args[1]) && !(callInfo.Flags & CallFlags_New))
            {
                // By ES2015 21.1.1.1 step 2, calling the String constructor directly results in an explicit ToString, which does not throw.
                return JavascriptSymbol::ToString(JavascriptSymbol::FromVar(args[1])->GetValue(), scriptContext);
                // Calling with new is an implicit ToString on the Symbol, resulting in a throw. For this case we can let JavascriptConversion handle the call.
            }
            str = JavascriptConversion::ToString(args[1], scriptContext);
        }
        else
        {
            str = scriptContext->GetLibrary()->GetEmptyString();
        }

        if (callInfo.Flags & CallFlags_New)
        {
            result = scriptContext->GetLibrary()->CreateStringObject(str);
        }
        else
        {
            result = str;
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(result), nullptr, scriptContext) :
            result;
    }

    // static
    bool IsValidCharCount(size_t charCount)
    {
        return charCount <= JavascriptString::MaxCharLength;
    }

    JavascriptString::JavascriptString(StaticType * type)
        : RecyclableObject(type), m_charLength(0), m_pszValue(0)
    {
        Assert(type->GetTypeId() == TypeIds_String);
    }

    JavascriptString::JavascriptString(StaticType * type, charcount_t charLength, const wchar_t* szValue)
        : RecyclableObject(type), m_charLength(charLength), m_pszValue(szValue)
    {
        Assert(type->GetTypeId() == TypeIds_String);
        AssertMsg(IsValidCharCount(charLength), "String length is out of range");
    }

    _Ret_range_(m_charLength, m_charLength)
    charcount_t JavascriptString::GetLength() const
    {
        return m_charLength;
    }

    int JavascriptString::GetLengthAsSignedInt() const
    {
        Assert(IsValidCharCount(m_charLength));
        return static_cast<int>(m_charLength);
    }

    const wchar_t* JavascriptString::UnsafeGetBuffer() const
    {
        return m_pszValue;
    }

    void JavascriptString::SetLength(charcount_t newLength)
    {
        if (!IsValidCharCount(newLength))
        {
            JavascriptExceptionOperators::ThrowOutOfMemory(this->GetScriptContext());
        }
        m_charLength = newLength;
    }

    void JavascriptString::SetBuffer(const wchar_t* buffer)
    {
        m_pszValue = buffer;
    }

    bool JavascriptString::IsValidIndexValue(charcount_t idx) const
    {
        return IsValidCharCount(idx) && idx < GetLength();
    }

    bool JavascriptString::Is(Var aValue)
    {
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_String;
    }

    JavascriptString* JavascriptString::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptString'");

        return static_cast<JavascriptString *>(RecyclableObject::FromVar(aValue));
    }

    charcount_t
    JavascriptString::GetBufferLength(const wchar_t * content)
    {
        size_t cchActual = wcslen(content);

#if defined(_M_X64_OR_ARM64)
        if (!IsValidCharCount(cchActual))
        {
            // Limit javascript string to 31-bit length
            Js::Throw::OutOfMemory();
        }
#else
        // There shouldn't be enought mamory to have UINT_MAX character.
        // INT_MAX is the upper bound for 32-bit;
        Assert(IsValidCharCount(cchActual));
#endif
        return static_cast<charcount_t>(cchActual);
    }

    charcount_t
    JavascriptString::GetBufferLength(
        const wchar_t * content,                     // Value to examine
        int charLengthOrMinusOne)                    // Optional length, in characters
    {
        //
        // Determine the actual length, in characters, not including a terminating '\0':
        // - If a length was not specified (charLength < 0), search for a terminating '\0'.
        //

        charcount_t cchActual;
        if (charLengthOrMinusOne < 0)
        {
            AssertMsg(charLengthOrMinusOne == -1, "The only negative value allowed is -1");
            cchActual = GetBufferLength(content);
        }
        else
        {
            cchActual = static_cast<charcount_t>(charLengthOrMinusOne);
        }
#ifdef CHECK_STRING
        // removed this to accommodate much larger string constant in regex-dna.js
        if (cchActual > 64 * 1024)
        {
            //
            // String was probably not '\0' terminated:
            // - We need to validate that the string's contents always fit within 1 GB to avoid
            //   overflow checking on 32-bit when using 'int' for 'byte *' pointer operations.
            //

            Throw::OutOfMemory();  // TODO: determine argument error
        }
#endif
        return cchActual;
    }

    template< size_t N >
    Var JavascriptString::StringBracketHelper(Arguments args, ScriptContext *scriptContext, const wchar_t(&tag)[N])
    {
        CompileAssert(0 < N && N <= JavascriptString::MaxCharLength);
        return StringBracketHelper(args, scriptContext, tag, static_cast<charcount_t>(N - 1), nullptr, 0);
    }

    template< size_t N1, size_t N2 >
    Var JavascriptString::StringBracketHelper(Arguments args, ScriptContext *scriptContext, const wchar_t(&tag)[N1], const wchar_t(&prop)[N2])
    {
        CompileAssert(0 < N1 && N1 <= JavascriptString::MaxCharLength);
        CompileAssert(0 < N2 && N2 <= JavascriptString::MaxCharLength);
        return StringBracketHelper(args, scriptContext, tag, static_cast<charcount_t>(N1 - 1), prop, static_cast<charcount_t>(N2 - 1));
    }

    BOOL JavascriptString::BufferEquals(__in_ecount(otherLength) LPCWSTR otherBuffer, __in charcount_t otherLength)
    {
        return otherLength == this->GetLength() &&
            JsUtil::CharacterBuffer<WCHAR>::StaticEquals(this->GetString(), otherBuffer, otherLength);
    }

    BOOL JavascriptString::HasItemAt(charcount_t index)
    {
        return IsValidIndexValue(index);
    }

    BOOL JavascriptString::GetItemAt(charcount_t index, Var* value)
    {
        if (!IsValidIndexValue(index))
        {
            return false;
        }

        wchar_t character = GetItem(index);

        *value = this->GetLibrary()->GetCharStringCache().GetStringForChar(character);

        return true;
    }

    wchar_t JavascriptString::GetItem(charcount_t index)
    {
        AssertMsg( IsValidIndexValue(index), "Must specify valid character");

        const wchar_t *str = this->GetString();
        return str[index];
    }

    void JavascriptString::CopyHelper(__out_ecount(countNeeded) wchar_t *dst, __in_ecount(countNeeded) const wchar_t * str, charcount_t countNeeded)
    {
        switch(countNeeded)
        {
        case 0:
            return;
        case 1:
            dst[0] = str[0];
            break;
        case 3:
            dst[2] = str[2];
            goto case_2;
        case 5:
            dst[4] = str[4];
            goto case_4;
        case 7:
            dst[6] = str[6];
            goto case_6;
        case 9:
            dst[8] = str[8];
            goto case_8;

        case 10:
            *(uint32 *)(dst+8) = *(uint32*)(str+8);
            // FALLTHROUGH
        case 8:
case_8:
            *(uint32 *)(dst+6) = *(uint32*)(str+6);
            // FALLTHROUGH
        case 6:
case_6:
            *(uint32 *)(dst+4) = *(uint32*)(str+4);
            // FALLTHROUGH
        case 4:
case_4:
            *(uint32 *)(dst+2) = *(uint32*)(str+2);
            // FALLTHROUGH
        case 2:
case_2:
            *(uint32 *)(dst) = *(uint32*)str;
            break;

        default:
            js_memcpy_s(dst, sizeof(wchar_t) * countNeeded, str, sizeof(wchar_t) * countNeeded);
        }
    }

    __inline JavascriptString* JavascriptString::ConcatDestructive(JavascriptString* pstRight)
    {
        Assert(pstRight);

        if(!IsFinalized())
        {
            if(CompoundString::Is(this))
            {
                return ConcatDestructive_Compound(pstRight);
            }

            if(VirtualTableInfo<ConcatString>::HasVirtualTable(this))
            {
                JavascriptString *const s = ConcatDestructive_ConcatToCompound(pstRight);
                if(s)
                {
                    return s;
                }
            }
        }
        else
        {
            const CharCount leftLength = GetLength();
            const CharCount rightLength = pstRight->GetLength();
            if(leftLength == 0 || rightLength == 0)
            {
                return ConcatDestructive_OneEmpty(pstRight);
            }

            if(CompoundString::ShouldAppendChars(leftLength) && CompoundString::ShouldAppendChars(rightLength))
            {
                return ConcatDestructive_CompoundAppendChars(pstRight);
            }
        }

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_ConcatTree);
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::ConcatDestructive(\"%.8s%s\") - creating ConcatString\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        return ConcatString::New(this, pstRight);
    }

    JavascriptString* JavascriptString::ConcatDestructive_Compound(JavascriptString* pstRight)
    {
        Assert(CompoundString::Is(this));
        Assert(pstRight);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::ConcatDestructive(\"%.8s%s\") - appending to CompoundString\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        CompoundString *const leftCs = CompoundString::FromVar(this);
        leftCs->PrepareForAppend();
        leftCs->Append(pstRight);
        return this;
    }

    JavascriptString* JavascriptString::ConcatDestructive_ConcatToCompound(JavascriptString* pstRight)
    {
        Assert(VirtualTableInfo<ConcatString>::HasVirtualTable(this));
        Assert(pstRight);

        const ConcatString *const leftConcatString = static_cast<const ConcatString *>(this);
        JavascriptString *const leftLeftString = leftConcatString->LeftString();
        if(VirtualTableInfo<ConcatString>::HasVirtualTable(leftLeftString))
        {
#ifdef PROFILE_STRINGS
            StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
            if(PHASE_TRACE_StringConcat)
            {
                Output::Print(
                    L"JavascriptString::ConcatDestructive(\"%.8s%s\") - converting ConcatString to CompoundString\n",
                    pstRight->IsFinalized() ? pstRight->GetString() : L"",
                    !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
                Output::Flush();
            }

            const ConcatString *const leftLeftConcatString = static_cast<const ConcatString *>(leftConcatString->LeftString());
            CompoundString *const cs = CompoundString::NewWithPointerCapacity(8, GetLibrary());
            cs->Append(leftLeftConcatString->LeftString());
            cs->Append(leftLeftConcatString->RightString());
            cs->Append(leftConcatString->RightString());
            cs->Append(pstRight);
            return cs;
        }
        return nullptr;
    }

    JavascriptString* JavascriptString::ConcatDestructive_OneEmpty(JavascriptString* pstRight)
    {
        Assert(pstRight);
        Assert(GetLength() == 0 || pstRight->GetLength() == 0);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength());
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::ConcatDestructive(\"%.8s%s\") - one side empty, using other side\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        if(GetLength() == 0)
        {
            return CompoundString::GetImmutableOrScriptUnreferencedString(pstRight);
        }
        Assert(CompoundString::GetImmutableOrScriptUnreferencedString(this) == this);
        return this;
    }

    JavascriptString* JavascriptString::ConcatDestructive_CompoundAppendChars(JavascriptString* pstRight)
    {
        Assert(pstRight);
        Assert(
            GetLength() != 0 &&
            pstRight->GetLength() != 0 &&
            (CompoundString::ShouldAppendChars(GetLength()) || CompoundString::ShouldAppendChars(pstRight->GetLength())));

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::ConcatDestructive(\"%.8s%s\") - creating CompoundString, appending chars\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        CompoundString *const cs = CompoundString::NewWithPointerCapacity(4, GetLibrary());
        cs->AppendChars(this);
        cs->AppendChars(pstRight);
        return cs;
    }

    __inline JavascriptString* JavascriptString::Concat(JavascriptString* pstLeft, JavascriptString* pstRight)
    {
        AssertMsg(pstLeft != nullptr, "Must have a valid left string");
        AssertMsg(pstRight != nullptr, "Must have a valid right string");

        if(!pstLeft->IsFinalized())
        {
            if(CompoundString::Is(pstLeft))
            {
                return Concat_Compound(pstLeft, pstRight);
            }

            if(VirtualTableInfo<ConcatString>::HasVirtualTable(pstLeft))
            {
                return Concat_ConcatToCompound(pstLeft, pstRight);
            }
        }
        else if(pstLeft->GetLength() == 0 || pstRight->GetLength() == 0)
        {
            return Concat_OneEmpty(pstLeft, pstRight);
        }

        if(pstLeft->GetLength() != 1 || pstRight->GetLength() != 1)
        {
#ifdef PROFILE_STRINGS
            StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_ConcatTree);
#endif
            if(PHASE_TRACE_StringConcat)
            {
                Output::Print(
                    L"JavascriptString::Concat(\"%.8s%s\") - creating ConcatString\n",
                    pstRight->IsFinalized() ? pstRight->GetString() : L"",
                    !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
                Output::Flush();
            }

            return ConcatString::New(pstLeft, pstRight);
        }

        return Concat_BothOneChar(pstLeft, pstRight);
    }

    JavascriptString* JavascriptString::Concat_Compound(JavascriptString * pstLeft, JavascriptString * pstRight)
    {
        Assert(pstLeft);
        Assert(CompoundString::Is(pstLeft));
        Assert(pstRight);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::Concat(\"%.8s%s\") - cloning CompoundString, appending to clone\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        // This is not a left-dead concat, but we can reuse available space in the left string
        // because it may be accessible by script code, append to a clone.
        const bool needAppend = pstRight->GetLength() != 0;
        CompoundString *const leftCs = CompoundString::FromVar(pstLeft)->Clone(needAppend);
        if(needAppend)
        {
            leftCs->Append(pstRight);
        }
        return leftCs;
    }

    JavascriptString* JavascriptString::Concat_ConcatToCompound(JavascriptString * pstLeft, JavascriptString * pstRight)
    {
        Assert(pstLeft);
        Assert(VirtualTableInfo<ConcatString>::HasVirtualTable(pstLeft));
        Assert(pstRight);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::Concat(\"%.8s%s\") - converting ConcatString to CompoundString\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        const ConcatString *const leftConcatString = static_cast<const ConcatString *>(pstLeft);
        CompoundString *const cs = CompoundString::NewWithPointerCapacity(8, pstLeft->GetLibrary());
        cs->Append(leftConcatString->LeftString());
        cs->Append(leftConcatString->RightString());
        cs->Append(pstRight);
        return cs;
    }

    JavascriptString* JavascriptString::Concat_OneEmpty(JavascriptString * pstLeft, JavascriptString * pstRight)
    {
        Assert(pstLeft);
        Assert(pstRight);
        Assert(pstLeft->GetLength() == 0 || pstRight->GetLength() == 0);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength());
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::Concat(\"%.8s%s\") - one side empty, using other side\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        if(pstLeft->GetLength() == 0)
        {
            return CompoundString::GetImmutableOrScriptUnreferencedString(pstRight);
        }
        Assert(CompoundString::GetImmutableOrScriptUnreferencedString(pstLeft) == pstLeft);
        return pstLeft;
    }

    JavascriptString* JavascriptString::Concat_BothOneChar(JavascriptString * pstLeft, JavascriptString * pstRight)
    {
        Assert(pstLeft);
        Assert(pstLeft->GetLength() == 1);
        Assert(pstRight);
        Assert(pstRight->GetLength() == 1);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_BufferString);
#endif
        if(PHASE_TRACE_StringConcat)
        {
            Output::Print(
                L"JavascriptString::Concat(\"%.8s%s\") - both sides length 1, creating BufferStringBuilder::WritableString\n",
                pstRight->IsFinalized() ? pstRight->GetString() : L"",
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? L"..." : L"");
            Output::Flush();
        }

        ScriptContext* scriptContext = pstLeft->GetScriptContext();
        BufferStringBuilder builder(2, scriptContext);
        wchar_t * stringBuffer = builder.DangerousGetWritableBuffer();
        stringBuffer[0] = *pstLeft->GetString();
        stringBuffer[1] = *pstRight->GetString();
        return builder.ToString();
    }

    Var JavascriptString::EntryCharAt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // General algorithm:
        //  1.  Call CheckObjectCoercible passing the this value as its argument.
        //  2.  Let S be the result of calling ToString, giving it the this value as its argument.
        //  3.  Let position be ToInteger(pos).
        //  4.  Let size be the number of characters in S.
        //  5.  If position < 0 or position = size, return the empty string.
        //  6.  Return a string of length 1, containing one character from S, where the first (leftmost) character in S is considered to be at position 0, the next one at position 1, and so on.
        //  NOTE
        //  The charAt function is intentionally generic; it does not require that its this value be a String object. Therefore, it can be transferred to other kinds of objects for use as a method.
        //

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.charAt", &pThis);

        charcount_t idxPosition = 0;
        if (args.Info.Count > 1)
        {
            idxPosition = ConvertToIndex(args[1], scriptContext);
        }

        //
        // Get the character at the specified position.
        //

        Var value;
        if (pThis->GetItemAt(idxPosition, &value))
        {
            return value;
        }
        else
        {
            return scriptContext->GetLibrary()->GetEmptyString();
        }
    }


    Var JavascriptString::EntryCharCodeAt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // General algorithm:
        // 1.  Call CheckObjectCoercible passing the this value as its argument.
        // 2.  Let S be the result of calling ToString, giving it the this value as its argument.
        // 3.  Let position be ToInteger(pos).
        // 4.  Let size be the number of characters in S.
        // 5.  If position < 0 or position = size, return NaN.
        // 6.  Return a value of Number type, whose value is the code unit value of the character at that position in the string S, where the first (leftmost) character in S is considered to be at position 0, the next one at position 1, and so on.
        // NOTE
        // The charCodeAt function is intentionally generic; it does not require that its this value be a String object. Therefore it can be transferred to other kinds of objects for use as a method.
        //
        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.charCodeAt", &pThis);

        charcount_t idxPosition = 0;
        if (args.Info.Count > 1)
        {
            idxPosition = ConvertToIndex(args[1], scriptContext);
        }

        //
        // Get the character at the specified position.
        //

        charcount_t charLength = pThis->GetLength();
        if (idxPosition >= charLength)
        {
            return scriptContext->GetLibrary()->GetNaN();
        }

        return TaggedInt::ToVarUnchecked(pThis->GetItem(idxPosition));
    }

    Var JavascriptString::EntryCodePointAt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.codePointAt", &pThis);

        charcount_t idxPosition = 0;
        if (args.Info.Count > 1)
        {
            idxPosition = ConvertToIndex(args[1], scriptContext);
        }

        charcount_t charLength = pThis->GetLength();
        if (idxPosition >= charLength)
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }

        // A surrogate pair consists of two characters, a lower part and a higher part.
        // Lower part is in range [0xD800 - 0xDBFF], while the higher is [0xDC00 - 0xDFFF].
        wchar_t first = pThis->GetItem(idxPosition);
        if (first >= 0xD800u && first < 0xDC00u && (uint)(idxPosition + 1) < pThis->GetLength())
        {
            wchar_t second = pThis->GetItem(idxPosition + 1);
            if (second >= 0xDC00 && second < 0xE000)
            {
                return TaggedInt::ToVarUnchecked(NumberUtilities::SurrogatePairAsCodePoint(first, second));
            }
        }
        return TaggedInt::ToVarUnchecked(first);
    }

    Var JavascriptString::EntryConcat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // General algorithm:
        // 1.  Call CheckObjectCoercible passing the this value as its argument.
        // 2.  Let S be the result of calling ToString, giving it the this value as its argument.
        // 3.  Let args be an internal list that is a copy of the argument list passed to this function.
        // 4.  Let R be S.
        // 5.  Repeat, while args is not empty
        //     Remove the first element from args and let next be the value of that element.
        //     Let R be the string value consisting of the characters in the previous value of R followed by the characters of ToString(next).
        // 6.  Return R.
        //
        // NOTE
        // The concat function is intentionally generic; it does not require that its this value be a String object. Therefore it can be transferred to other kinds of objects for use as a method.
        //

        if(args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.prototype.concat");
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");
        if (!JavascriptConversion::CheckObjectCoercible(args[0], scriptContext))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"String.prototype.concat");
        }

        JavascriptString* pstr = nullptr;
        JavascriptString* accum = nullptr;
        for (uint index = 0; index < args.Info.Count; index++)
        {
            if (JavascriptString::Is(args[index]))
            {
                pstr = JavascriptString::FromVar(args[index]);
            }
            else
            {
                pstr = JavascriptConversion::ToString(args[index], scriptContext);
            }

            if (index == 0)
            {
                accum = pstr;
            }
            else
            {
                accum = Concat(accum,pstr);
            }

        }

        return accum;
    }

    Var JavascriptString::EntryFromCharCode(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // Construct a new string instance to contain all of the explicit parameters:
        // - Don't include the 'this' parameter.
        //
        if(args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.fromCharCode");
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        int charLength = args.Info.Count - 1;

        // Special case for single char
        if( charLength == 1 )
        {
            wchar_t ch = JavascriptConversion::ToUInt16(args[1], scriptContext);
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(ch);
        }

        BufferStringBuilder builder(charLength,scriptContext);
        wchar_t * stringBuffer = builder.DangerousGetWritableBuffer();

        //
        // Call ToUInt16 for each parameter, storing the character at the appropriate position.
        //

        for (uint idxArg = 1; idxArg < args.Info.Count; idxArg++)
        {
            *stringBuffer++ = JavascriptConversion::ToUInt16(args[idxArg], scriptContext);
        }

        //
        // Return the new string instance.
        //
        return builder.ToString();
    }

    Var JavascriptString::EntryFromCodePoint(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Negative argument count");

        if (args.Info.Count <= 1)
        {
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else if (args.Info.Count == 2)
        {
            // Special case for a single char string formed from only code point in range [0x0, 0xFFFF]
            double num = JavascriptConversion::ToNumber(args[1], scriptContext);

            if (!NumberUtilities::IsFinite(num))
            {
                JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidCodePoint);
            }

            if (num < 0 || num > 0x10FFFF || floor(num) != num)
            {
                JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidCodePoint, Js::JavascriptConversion::ToString(args[1], scriptContext)->GetSz());
            }

            if (num < 0x10000)
            {
                return scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar((uint16)num);
            }
        }

        BEGIN_TEMP_ALLOCATOR(tempAllocator, scriptContext, L"fromCodePoint");
        // Create a temporary buffer that is double the arguments count (in case all are surrogate pairs)
        size_t bufferLength = (args.Info.Count - 1) * 2;
        wchar_t *tempBuffer = AnewArray(tempAllocator, wchar_t, bufferLength);
        uint32 count = 0;

        for (uint i = 1; i < args.Info.Count; i++)
        {
            double num = JavascriptConversion::ToNumber(args[i], scriptContext);

            if (!NumberUtilities::IsFinite(num))
            {
                JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidCodePoint);
            }

            if (num < 0 || num > 0x10FFFF || floor(num) != num)
            {
                JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidCodePoint, Js::JavascriptConversion::ToString(args[i], scriptContext)->GetSz());
            }

            if (num < 0x10000)
            {
                __analysis_assume(count < bufferLength);
                Assert(count < bufferLength);
#pragma prefast(suppress: 22102, "I have an assert in place to guard against overflow. Even though this should never happen.")
                tempBuffer[count] = (wchar_t)num;
                count++;
            }
            else
            {
                __analysis_assume(count + 1 < bufferLength);
                Assert(count  + 1 < bufferLength);
                NumberUtilities::CodePointAsSurrogatePair((codepoint_t)num, (tempBuffer + count), (tempBuffer + count + 1));
                count += 2;
            }
        }
        // Create a string of appropriate length
        __analysis_assume(count <= bufferLength);
        Assert(count <= bufferLength);
        JavascriptString *toReturn = JavascriptString::NewCopyBuffer(tempBuffer, count, scriptContext);


        END_TEMP_ALLOCATOR(tempAllocator, scriptContext);
        return toReturn;
    }

    Var JavascriptString::EntryIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        return JavascriptNumber::ToVar(IndexOf(args, scriptContext, L"String.prototype.indexOf", true), scriptContext);
    }

    int JavascriptString::IndexOf(ArgumentReader& args, ScriptContext* scriptContext, const wchar_t* apiNameForErrorMsg, bool isRegExpAnAllowedArg)
    {
        // The algorithm steps in the spec are the same between String.prototype.indexOf and
        // String.prototype.includes, except that includes returns true if an index is found,
        // false otherwise.  Share the implementation between these two APIs.
        //
        // 1.  Call CheckObjectCoercible passing the this value as its argument.
        // 2.  Let S be the result of calling ToString, giving it the this value as its argument.
        // 3.  Let searchStr be ToString(searchString).
        // 4.  Let pos be ToInteger(position). (If position is undefined, this step produces the value 0).
        // 5.  Let len be the number of characters in S.
        // 6.  Let start be min(max(pos, 0), len).
        // 7.  Let searchLen be the number of characters in searchStr.
        // 8.  Return the smallest possible integer k not smaller than start such that k+ searchLen is not greater than len, and for all nonnegative integers j less than searchLen, the character at position k+j of S is the same as the character at position j of searchStr); but if there is no such integer k, then return the value -1.
        // NOTE
        // The indexOf function is intentionally generic; it does not require that its this value be a String object. Therefore, it can be transferred to other kinds of objects for use as a method.
        //

        JavascriptString * pThis;
        JavascriptString * searchString;

        GetThisAndSearchStringArguments(args, scriptContext, apiNameForErrorMsg, &pThis, &searchString, isRegExpAnAllowedArg);

        int len = pThis->GetLength();
        int searchLen = searchString->GetLength();

        int position = 0;

        if (args.Info.Count > 2)
        {
            if (JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {
                position = 0;
            }
            else
            {
                position = ConvertToIndex(args[2], scriptContext); // this is to adjust corner cases like MAX_VALUE
                position = min(max(position, 0), len);  // adjust position within string limits
            }
        }

        // Zero length search strings are always found at the current search position
        if (searchLen == 0)
        {
            return position;
        }

        int result = -1;

        if (position < pThis->GetLengthAsSignedInt())
        {
            const wchar_t* searchStr = searchString->GetString();
            const wchar_t* inputStr = pThis->GetString();
            if (searchLen == 1)
            {
                int i = position;
                for(; i < len && inputStr[i] != *searchStr ; i++);
                if (i < len)
                {
                    result = i;
                }
            }
            else
            {
                JmpTable jmpTable;
                bool fAsciiJumpTable = BuildLastCharForwardBoyerMooreTable(jmpTable, searchStr, searchLen);
                if (!fAsciiJumpTable)
                {
                    result = JavascriptString::strstr(pThis, searchString, false, position);
                }
                else
                {
                    result = IndexOfUsingJmpTable(jmpTable, inputStr, len, searchStr, searchLen, position);
                }
            }
        }
        return result;
    }

    Var JavascriptString::EntryLastIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // 1.  Call CheckObjectCoercible passing the this value as its argument.
        // 2.  Let S be the result of calling ToString, giving it the this value as its argument.
        // 3.  Let searchStr be ToString(searchString).
        // 4.  Let numPos be ToNumber(position). (If position is undefined, this step produces the value NaN).
        // 5.  If numPos is NaN, let pos be +?; otherwise, let pos be ToInteger(numPos).
        // 6.  Let len be the number of characters in S.
        // 7.  Let start min(max(pos, 0), len).
        // 8.  Let searchLen be the number of characters in searchStr.
        // 9.  Return the largest possible nonnegative integer k not larger than start such that k+ searchLen is not greater than len, and for all nonnegative integers j less than searchLen, the character at position k+j of S is the same as the character at position j of searchStr; but if there is no such integer k, then return the value -1.
        // NOTE
        // The lastIndexOf function is intentionally generic; it does not require that its this value be a String object. Therefore, it can be transferred to other kinds of objects for use as a method.
        //

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.lastIndexOf", &pThis);

        // default search string if the search argument is not provided
        JavascriptString * searchArg;
        if(args.Info.Count > 1)
        {
            if (JavascriptString::Is(args[1]))
            {
                searchArg = JavascriptString::FromVar(args[1]);
            }
            else
            {
                searchArg = JavascriptConversion::ToString(args[1], scriptContext);
            }
        }
        else
        {
            searchArg = scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }

        wchar_t const * const inputStr = pThis->GetString();
        const wchar_t * const searchStr = searchArg->GetString();
        const int len = pThis->GetLength();
        int position = len;
        int const searchLen = searchArg->GetLength();

        // Determine if the main string can't contain the search string by length
        if ( searchLen > len )
        {
            return JavascriptNumber::ToVar(-1, scriptContext);
        }

        if(args.Info.Count > 2)
        {
            double pos = JavascriptConversion::ToNumber(args[2], scriptContext);
            if (!JavascriptNumber::IsNan(pos))
            {
                pos = JavascriptConversion::ToInteger(pos);
                position = (int)min(max(pos, (double)0), (double)len); // adjust position within string limits
            }
        }

        int result = -1;
        // Zero length search strings are always found at the current search position
        if ( searchLen == 0 )
        {
            return JavascriptNumber::ToVar(position, scriptContext);
        }
        else if (searchLen == 1)
        {
            wchar_t const * current = inputStr + min(position,len - 1);
            while (*current != *searchStr)
            {
                current--;
                if (current < inputStr)
                {
                    return JavascriptNumber::ToVar(result, scriptContext); //return -1
                }
            }
            result =  (int)(current - inputStr);
            return JavascriptNumber::ToVar(result, scriptContext);
        }

        // Structure for a partial ASCII Boyer-Moore
        JmpTable jmpTable;
        bool fAsciiJumpTable = BuildFirstCharBackwardBoyerMooreTable(jmpTable, searchStr, searchLen);

        if (!fAsciiJumpTable)
        {
            wchar_t const * start = inputStr;
            wchar_t const * current = inputStr + len - 1;
            wchar_t const * searchStrEnd = searchStr + searchLen - 1;
            while (current >= start)
            {
                wchar_t const * s1 = current;
                wchar_t const * s2 = searchStrEnd;

                while (s1 >= start && s2 >= searchStr && !(*s1 - *s2))
                {
                    s1--, s2--;
                }

                if (s2 < searchStr)
                {
                    result = (int)(current - start) - searchLen + 1;
                    return JavascriptNumber::ToVar(result, scriptContext);
                }

                current--;
            }
            return JavascriptNumber::ToVar(result, scriptContext);
        }
        else
        {
            result = LastIndexOfUsingJmpTable(jmpTable, inputStr, len, searchStr, searchLen, position);
        }
        return JavascriptNumber::ToVar(result, scriptContext);
    }

    // Performs common ES spec steps for getting this argument in string form:
    // 1. Let O be CHeckObjectCoercible(this value).
    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    void JavascriptString::GetThisStringArgument(ArgumentReader& args, ScriptContext* scriptContext, const wchar_t* apiNameForErrorMsg, JavascriptString** ppThis)
    {
        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, apiNameForErrorMsg);
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString * pThis;
        if (JavascriptString::Is(args[0]))
        {
            pThis = JavascriptString::FromVar(args[0]);
        }
        else
        {

            pThis = JavascriptConversion::CoerseString(args[0], scriptContext , apiNameForErrorMsg);

        }

        *ppThis = pThis;
    }

    // Performs common ES spec steps for getting this and first parameter arguments in string form:
    // 1. Let O be CHeckObjectCoercible(this value).
    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    // 4. Let otherStr be ToString(firstArg).
    // 5. ReturnIfAbrupt(otherStr).
    void JavascriptString::GetThisAndSearchStringArguments(ArgumentReader& args, ScriptContext* scriptContext, const wchar_t* apiNameForErrorMsg, JavascriptString** ppThis, JavascriptString** ppSearch, bool isRegExpAnAllowedArg)
    {
        GetThisStringArgument(args, scriptContext, apiNameForErrorMsg, ppThis);

        JavascriptString * pSearch = scriptContext->GetLibrary()->GetUndefinedDisplayString();
        if (args.Info.Count > 1)
        {
            if (!isRegExpAnAllowedArg && JavascriptRegExp::Is(args[1]))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, apiNameForErrorMsg);
            }
            else if (JavascriptString::Is(args[1]))
            {
                pSearch = JavascriptString::FromVar(args[1]);
            }
            else
            {
                pSearch = JavascriptConversion::ToString(args[1], scriptContext);
            }
        }

        *ppSearch = pSearch;
    }

    Var JavascriptString::EntryLocaleCompare(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if(args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.prototype.localeCompare");
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString * pThis;
        JavascriptString * pThat;

        GetThisAndSearchStringArguments(args, scriptContext, L"String.prototype.localeCompare", &pThis, &pThat, true);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->GetConfig()->IsIntlEnabled())
        {
            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {
                IntlEngineInterfaceExtensionObject* intlExtenionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                if (args.Info.Count == 2)
                {
                    auto undefined = scriptContext->GetLibrary()->GetUndefined();
                    CallInfo toPass(callInfo.Flags, 7);
                    return intlExtenionObject->EntryIntl_CompareString(function, toPass, undefined, pThis, pThat, undefined, undefined, undefined, undefined);
                }
                else
                {
                    JavascriptFunction* func = intlExtenionObject->GetStringLocaleCompare();
                    if (func)
                    {
                        return func->CallFunction(args);
                    }
                    // Initialize String.prototype.toLocaleCompare
                    scriptContext->GetLibrary()->InitializeIntlForStringPrototype();
                    func = intlExtenionObject->GetStringLocaleCompare();
                    if (func)
                    {
                        return func->CallFunction(args);
                    }
                    AssertMsg(false, "Intl code didn't initialized String.prototype.toLocaleCompare method.");
                }
            }
        }
#endif

        const wchar_t* pThisStr = pThis->GetString();
        int thisStrCount = pThis->GetLength();

        const wchar_t* pThatStr = pThat->GetString();
        int thatStrCount = pThat->GetLength();

        LCID lcid = GetUserDefaultLCID();
        int result = CompareStringW(lcid, NULL, pThisStr, thisStrCount, pThatStr, thatStrCount );
        if (result == 0)
        {
            // TODO there is no spec on the error thrown here.
            // When the support for HR errors is implemented replace this with the same error reported by v5.8
            JavascriptError::ThrowRangeError(function->GetScriptContext(),
                VBSERR_InternalError /* TODO-ERROR: L"Failed compare operation"*/ );
        }
        return JavascriptNumber::ToVar(result-2, scriptContext);
    }

    Var JavascriptString::EntryMatch(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // TODO: Move argument processing into DirectCall with proper handling.
        //

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.match", &pThis);

        JavascriptRegExp * pRegEx;
        if (args.Info.Count == 1)
        {
            //Use an empty regex to match against, if no argument was supplied into "match"
            pRegEx = scriptContext->GetLibrary()->CreateEmptyRegExp();
        }
        else if (JavascriptRegExp::Is(args[1]))
        {
            pRegEx = JavascriptRegExp::FromVar(args[1]);
        }
        else
        {
            Var aCompiledRegex = NULL;
            Var thisRegex = JavascriptRegExp::OP_NewRegEx(aCompiledRegex, scriptContext);
            pRegEx = JavascriptRegExp::FromVar(JavascriptRegExp::NewInstance(function, 2, thisRegex, args[1]));
        }

        return RegexHelper::RegexMatch(scriptContext, pRegEx, pThis, RegexHelper::IsResultNotUsed(callInfo.Flags));
    }

    Var JavascriptString::EntryNormalize(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString *pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.normalize", &pThis);

        NORM_FORM form = NORM_FORM::NormalizationC;

        if (args.Info.Count >= 2 && !(JavascriptOperators::IsUndefinedObject(args.Values[1])))
        {
            JavascriptString *formStr = nullptr;
            if (JavascriptString::Is(args[1]))
            {
                formStr = JavascriptString::FromVar(args[1]);
            }
            else
            {
                formStr = JavascriptConversion::ToString(args[1], scriptContext);
            }

            if (formStr->BufferEquals(L"NFD", 3))
            {
                form = NORM_FORM::NormalizationD;
            }
            else if (formStr->BufferEquals(L"NFKC", 4))
            {
                form = NORM_FORM::NormalizationKC;
            }
            else if (formStr->BufferEquals(L"NFKD", 4))
            {
                form = NORM_FORM::NormalizationKD;
            }
            else if (!formStr->BufferEquals(L"NFC", 3))
            {
                JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidNormalizationForm, formStr->GetString());
            }
        }

        if (IsNormalizedString(form, pThis->GetSz(), pThis->GetLength()))
        {
            return JavascriptString::NewWithSz(pThis->GetSz(), scriptContext);
        }


        BEGIN_TEMP_ALLOCATOR(tempAllocator, scriptContext, L"normalize");

        charcount_t sizeEstimate = 0;
        wchar_t* buffer = pThis->GetNormalizedString(form, tempAllocator, sizeEstimate);
        JavascriptString * retVal;
        if (buffer == nullptr)
        {
            Assert(sizeEstimate == 0);
            retVal = scriptContext->GetLibrary()->GetEmptyString();
        }
        else
        {
            retVal = JavascriptString::NewCopyBuffer(buffer, sizeEstimate, scriptContext);
        }

        END_TEMP_ALLOCATOR(tempAllocator, scriptContext);
        return retVal;
    }

    ///----------------------------------------------------------------------------
    /// String.raw(), as described in (ES6.0 (Draft 18): S21.1.2.4).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryRaw(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 2)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"String.raw");
        }

        RecyclableObject* callSite;
        RecyclableObject* raw;
        Var rawVar;

        // Call ToObject on the first argument to get the callSite (which is also cooked string array)
        // ToObject returns false if the parameter is null or undefined
        if (!JavascriptConversion::ToObject(args[1], scriptContext, &callSite))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"String.raw");
        }

        // Get the raw property from the callSite object
        if (!callSite->GetProperty(callSite, Js::PropertyIds::raw, &rawVar, nullptr, scriptContext))
        {
            rawVar = scriptContext->GetLibrary()->GetUndefined();
        }

        if (!JavascriptConversion::ToObject(rawVar, scriptContext, &raw))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, L"String.raw");
        }

        int64 length = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(raw, scriptContext), scriptContext);

        // If there are no raw strings (somehow), return empty string
        if (length <= 0)
        {
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        // Get the first raw string
        Var var = JavascriptOperators::OP_GetElementI_UInt32(raw, 0, scriptContext);
        JavascriptString* string = JavascriptConversion::ToString(var, scriptContext);

        // If there is only one raw string, just return that one raw string (doesn't matter if there are replacements)
        if (length == 1)
        {
            return string;
        }

        // We aren't going to bail early so let's create a StringBuilder and put the first raw string in there
        CompoundString::Builder<64 * sizeof(void *) / sizeof(wchar_t)> stringBuilder(scriptContext);
        stringBuilder.Append(string);

        // Each raw string is followed by a substitution expression except for the last one
        // We will always have one more string constant than substitution expression
        // `strcon1 ${expr1} strcon2 ${expr2} strcon3` = strcon1 + expr1 + strcon2 + expr2 + strcon3
        //
        // strcon1 --- step 1 (above)
        // expr1   \__ step 2
        // strcon2 /
        // expr2   \__ step 3
        // strcon3 /
        for (uint32 i = 1; i < length; ++i)
        {
            // First append the next substitution expression
            // If we have an arg at [i+1] use that one, otherwise empty string (which is nop)
            if (i+1 < args.Info.Count)
            {
                string = JavascriptConversion::ToString(args[i+1], scriptContext);

                stringBuilder.Append(string);
            }

            // Then append the next string (this will also cover the final string case)
            var = JavascriptOperators::OP_GetElementI_UInt32(raw, i, scriptContext);
            string = JavascriptConversion::ToString(var, scriptContext);

            stringBuilder.Append(string);
        }

        // CompoundString::Builder has saved our lives
        return stringBuilder.ToString();
    }

    Var JavascriptString::EntryReplace(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, L"String.prototype.replace");

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // TODO: Move argument processing into DirectCall with proper handling.
        //

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.replace", &pThis);

        JavascriptRegExp * pRegEx = nullptr;
        JavascriptString * pMatch = nullptr;

        JavascriptString * pReplace = nullptr;
        JavascriptFunction* replacefn = nullptr;

        SearchValueHelper(scriptContext, ((args.Info.Count > 1)?args[1]:scriptContext->GetLibrary()->GetNull()), &pRegEx, &pMatch);
        ReplaceValueHelper(scriptContext, ((args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined()), &replacefn, &pReplace);

        if (pRegEx != nullptr)
        {
            if (replacefn != nullptr)
            {
                return RegexHelper::RegexReplaceFunction(scriptContext, pRegEx, pThis, replacefn, nullptr);
            }
            else
            {
                return RegexHelper::RegexReplace(scriptContext, pRegEx, pThis, pReplace, nullptr, RegexHelper::IsResultNotUsed(callInfo.Flags));
            }
        }

        AssertMsg(pMatch != nullptr, "Match string shouldn't be null");
        if (replacefn != nullptr)
        {
            return RegexHelper::StringReplace(pMatch, pThis, replacefn);
        }
        else
        {
            if (callInfo.Flags & CallFlags_NotUsed)
            {
                return scriptContext->GetLibrary()->GetEmptyString();
            }
            return RegexHelper::StringReplace(pMatch, pThis, pReplace);
        }
    }

    void JavascriptString::SearchValueHelper(ScriptContext* scriptContext, Var aValue, JavascriptRegExp ** ppSearchRegEx, JavascriptString ** ppSearchString)
    {
        *ppSearchRegEx = nullptr;
        *ppSearchString = nullptr;

        if (JavascriptRegExp::Is(aValue))
        {
            *ppSearchRegEx = JavascriptRegExp::FromVar(aValue);
        }
        else if (JavascriptString::Is(aValue))
        {
            *ppSearchString = JavascriptString::FromVar(aValue);
        }
        else
        {
            *ppSearchString = JavascriptConversion::ToString(aValue, scriptContext);
        }
    }

    void JavascriptString::ReplaceValueHelper(ScriptContext* scriptContext, Var aValue, JavascriptFunction ** ppReplaceFn, JavascriptString ** ppReplaceString)
    {
        *ppReplaceFn = nullptr;
        *ppReplaceString = nullptr;

        if (JavascriptFunction::Is(aValue))
        {
            *ppReplaceFn = JavascriptFunction::FromVar(aValue);
        }
        else if (JavascriptString::Is(aValue))
        {
            *ppReplaceString = JavascriptString::FromVar(aValue);
        }
        else
        {
            *ppReplaceString = JavascriptConversion::ToString(aValue, scriptContext);
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// JavascriptString::EntrySearch
    ///
    /// When the search method is called with argument regexp the following steps are taken:
    ///    1. Call CheckObjectCoercible passing the this value as its argument.
    ///    2. Let string be the result of calling ToString, giving it the this value as its argument.
    ///    3. If Type(regexp) is Object and the value of the [[Class]] internal property of regexp is "RegExp", then let rx be regexp;
    ///    4. Else, let rx be a new RegExp object created as if by the expression new RegExp(regexp) where RegExp is the standard built-in constructor with that name.
    ///    5. Search the value string from its beginning for an occurrence of the regular expression pattern rx. Let result be a number indicating the offset within string where the pattern matched, or -1 if there was no match. The lastIndex and global properties of regexp are ignored when performing the search. The lastIndex property of regexp is left unchanged.
    ///    6. Return result.
    ///    NOTE The search function is intentionally generic; it does not require that its this value be a String object. Therefore, it can be transferred to other kinds of objects for use as a method.

    ///
    ///----------------------------------------------------------------------------

    Var JavascriptString::EntrySearch(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.search", &pThis);

        JavascriptRegExp * pRegEx = nullptr;
        if(args.Info.Count > 1)
        {
            if (JavascriptRegExp::Is(args[1]))
            {
                pRegEx = JavascriptRegExp::FromVar(args[1]);
            }
            else
            {
                pRegEx = JavascriptRegExp::CreateRegEx(args[1], nullptr, scriptContext);
            }
        }
        else
        {
            pRegEx = JavascriptRegExp::CreateRegEx(scriptContext->GetLibrary()->GetUndefined(), nullptr, scriptContext);
        }
        return RegexHelper::RegexSearch(scriptContext, pRegEx, pThis);

    }

    Var JavascriptString::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.slice", &pThis);

        int len = pThis->GetLength();

        int idxStart = 0;
        int idxEnd = len;

        if (args.Info.Count > 1)
        {
            idxStart = JavascriptOperators::IsUndefinedObject(args[1], scriptContext) ? 0 : ConvertToIndex(args[1], scriptContext);
            if (args.Info.Count > 2)
            {
                idxEnd = JavascriptOperators::IsUndefinedObject(args[2], scriptContext) ? len : ConvertToIndex(args[2], scriptContext);
            }
        }

        if (idxStart < 0)
        {
            idxStart = max(len + idxStart, 0);
        }
        else if (idxStart > len)
        {
            idxStart = len;
        }

        if (idxEnd < 0)
        {
            idxEnd = max(len + idxEnd, 0);
        }
        else if (idxEnd > len )
        {
            idxEnd = len;
        }

        if (idxEnd < idxStart)
        {
            idxEnd = idxStart;
        }
        return SubstringCore(pThis, idxStart, idxEnd - idxStart, scriptContext);
    }

    Var JavascriptString::EntrySplit(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString* input = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.split", &input);

        if (args.Info.Count == 1)
        {
            JavascriptArray* ary = scriptContext->GetLibrary()->CreateArray(1);
            ary->DirectSetItemAt(0, input);
            return ary;
        }
        else
        {
            uint32 limit;
            if (args.Info.Count < 3 || JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {
                limit = UINT_MAX;
            }
            else
            {
                limit = JavascriptConversion::ToUInt32(args[2], scriptContext);
            }

            if (JavascriptRegExp::Is(args[1]))
            {
                return RegexHelper::RegexSplit(scriptContext, JavascriptRegExp::FromVar(args[1]), input, limit,
                    RegexHelper::IsResultNotUsed(callInfo.Flags));
            }
            else
            {
                JavascriptString* separator = JavascriptConversion::ToString(args[1], scriptContext);

                if (callInfo.Flags & CallFlags_NotUsed)
                {
                    return scriptContext->GetLibrary()->GetNull();
                }

                if (!limit)
                {
                    JavascriptArray* ary = scriptContext->GetLibrary()->CreateArray(0);
                    return ary;
                }

                if (JavascriptOperators::GetTypeId(args[1]) == TypeIds_Undefined)
                {
                    JavascriptArray* ary = scriptContext->GetLibrary()->CreateArray(1);
                    ary->DirectSetItemAt(0, input);
                    return ary;
                }

                return RegexHelper::StringSplit(separator, input, limit);
            }
        }
    }

    Var JavascriptString::EntrySubstring(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.substring", &pThis);

        int len = pThis->GetLength();

        int idxStart = 0;
        int idxEnd = len;

        if (args.Info.Count > 1)
        {
            idxStart = JavascriptOperators::IsUndefinedObject(args[1], scriptContext) ? 0 : ConvertToIndex(args[1], scriptContext);
            if (args.Info.Count > 2)
            {
                idxEnd = JavascriptOperators::IsUndefinedObject(args[2], scriptContext) ? len : ConvertToIndex(args[2], scriptContext);
            }
        }

        idxStart = min(max(idxStart, 0), len);
        idxEnd = min(max(idxEnd, 0), len);
        if(idxEnd < idxStart)
        {
            //swap
            idxStart ^= idxEnd;
            idxEnd ^= idxStart;
            idxStart ^= idxEnd;
        }

        if (idxStart == 0 && idxEnd == len)
        {
            //return the string if we need to substring entire span
            return pThis;
        }

        return SubstringCore(pThis, idxStart, idxEnd - idxStart, scriptContext);
    }

    Var JavascriptString::EntrySubstr(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.substr", &pThis);

        int len = pThis->GetLength();

        int idxStart = 0;
        int idxEnd = len;

        if (args.Info.Count > 1)
        {
            idxStart = JavascriptOperators::IsUndefinedObject(args[1], scriptContext) ? 0 : ConvertToIndex(args[1], scriptContext);
            if (args.Info.Count > 2)
            {
                idxEnd = JavascriptOperators::IsUndefinedObject(args[2], scriptContext) ? len : ConvertToIndex(args[2], scriptContext);
            }
        }
        if (idxStart < 0)
        {
            idxStart = max(len + idxStart, 0);
        }
        else if (idxStart > len)
        {
            idxStart = len;
        }

        if (idxEnd < 0)
        {
            idxEnd = idxStart;
        }
        else if (idxEnd > len - idxStart)
        {
            idxEnd = len;
        }
        else
        {
            idxEnd += idxStart;
        }

        if (idxStart == 0 && idxEnd == len)
        {
            //return the string if we need to substr entire span
            return pThis;
        }

        Assert(0 <= idxStart && idxStart <= idxEnd && idxEnd <= len);
        return SubstringCore(pThis, idxStart, idxEnd - idxStart, scriptContext);
    }

    Var JavascriptString::SubstringCore(JavascriptString* pThis, int idxStart, int span, ScriptContext* scriptContext)
    {
        return SubString::New(pThis, idxStart, span);
    }

    Var JavascriptString::EntryToLocaleLowerCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        return ToLocaleCaseHelper(args[0], false, scriptContext);
    }

    Var JavascriptString::EntryToLocaleUpperCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        return ToLocaleCaseHelper(args[0], true, scriptContext);
    }

    Var JavascriptString::EntryToLowerCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.toLowerCase", &pThis);

        // Fast path for one character strings
        if (pThis->GetLength() == 1)
        {
            wchar_t inChar = pThis->GetString()[0];
            wchar_t outChar = CharToLowerCase(inChar);

            return (inChar == outChar) ? pThis : scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(outChar);
        }

        return ToCaseCore(pThis, ToLower);
    }

    Var JavascriptString::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if(args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.prototype.toString");
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString* str = nullptr;
        if (!GetThisValueVar(args[0], &str, scriptContext))
        {
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToString, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.prototype.toString");
        }

        return str;
    }

    Var JavascriptString::EntryToUpperCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.toUpperCase", &pThis);

        // Fast path for one character strings
        if (pThis->GetLength() == 1)
        {
            wchar_t inChar = pThis->GetString()[0];
            wchar_t outChar = CharToUpperCase(inChar);

            return (inChar == outChar) ? pThis : scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(outChar);
        }

        return ToCaseCore(pThis, ToUpper);
    }

    Var JavascriptString::ToCaseCore(JavascriptString* pThis, ToCase toCase)
    {
        charcount_t count = pThis->GetLength();

        BufferStringBuilder builder(count, pThis->type->GetScriptContext());
        const wchar_t *inStr = pThis->GetString();
        wchar_t *outStr = builder.DangerousGetWritableBuffer();

        wchar_t* outStrLim = outStr + count;

        //TODO the mapping based on unicode tables needs review for ES5.
        // mapping is not always 1.1
        if(toCase == ToUpper)
        {
            while (outStr < outStrLim)
            {
                *outStr++ = CharToUpperCase(*inStr++);
            }
        }
        else
        {
            Assert(toCase == ToLower);
            while(outStr < outStrLim)
            {
                *outStr++ = CharToLowerCase(*inStr++);
            }
        }

        return builder.ToString();
    }

    Var JavascriptString::EntryTrim(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(StringTrimCount);

        Assert(!(callInfo.Flags & CallFlags_New));

        //15.5.4.20      The following steps are taken:
        //1.    Call CheckObjectCoercible passing the this value as its argument.
        //2.    Let S be the result of calling ToString, giving it the this value as its argument.
        //3.    Let T be a string value that is a copy of S with both leading and trailing white space removed. The definition of white space is the union of WhiteSpace and LineTerminator.
        //4.    Return T.

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.trim", &pThis);
        return TrimLeftRightHelper<true /*trimLeft*/, true /*trimRight*/>(pThis, scriptContext);
    }

    Var JavascriptString::EntryTrimLeft(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // 1.Let O be  CheckObjectCoercible(this value) .
        // 2.Let S be  ToString(O) .
        // 3.ReturnIfAbrupt(S).
        // 4.Let T be a String value that is a copy of S with leading white space removed. The definition of white space is the union of WhiteSpace and )LineTerminator.
        //   When determining whether a Unicode code point is in Unicode general category "Zs", code unit sequences are interpreted as UTF-16 encoded code point sequences as specified in 6.1.4.
        // 5.Return T.

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.trimLeft", &pThis);
        return TrimLeftRightHelper< true /*trimLeft*/, false /*trimRight*/>(pThis, scriptContext);
    }


    Var JavascriptString::EntryTrimRight(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // 1.Let O be  CheckObjectCoercible(this value) .
        // 2.Let S be  ToString(O) .
        // 3.ReturnIfAbrupt(S).
        // 4.Let T be a String value that is a copy of S with trailing white space removed.The definition of white space is the union of WhiteSpace and )LineTerminator.
        //   When determining whether a Unicode code point is in Unicode general category "Zs", code unit sequences are interpreted as UTF - 16 encoded code point sequences as specified in 6.1.4.
        // 5.Return T.

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.trimRight", &pThis);
        return TrimLeftRightHelper<false /*trimLeft*/, true /*trimRight*/>(pThis, scriptContext);
    }

    template <bool trimLeft, bool trimRight>
    Var JavascriptString::TrimLeftRightHelper(JavascriptString* arg, ScriptContext* scriptContext)
    {
        static_assert(trimLeft || trimRight, "bad template instance of TrimLeftRightHelper()");

        int len = arg->GetLength();
        const wchar_t *string = arg->GetString();

        int idxStart = 0;
        if (trimLeft)
        {
            for (; idxStart < len; idxStart++)
            {
                wchar_t ch = string[idxStart];
                if (IsWhiteSpaceCharacter(ch))
                {
                    continue;
                }
                break;
            }

            if (len == idxStart)
            {
                return (scriptContext->GetLibrary()->GetEmptyString());
            }
        }

        int idxEnd = len - 1;
        if (trimRight)
        {
            for (; idxEnd >= 0; idxEnd--)
            {
                wchar_t ch = string[idxEnd];
                if (IsWhiteSpaceCharacter(ch))
                {
                    continue;
                }
                break;
            }

            if (!trimLeft)
            {
                if (idxEnd < 0)
                {
                    Assert(idxEnd == -1);
                    return (scriptContext->GetLibrary()->GetEmptyString());
                }
            }
            else
            {
                Assert(idxEnd >= 0);
            }
        }
        return SubstringCore(arg, idxStart, idxEnd - idxStart + 1, scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// Repeat() returns a new string equal to the toString(this) repeated n times,
    /// as described in (ES6.0: S21.1.3.13).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryRepeat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(RepeatCount);

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, L"String.prototype.repeat", &pThis);

        const wchar_t* thisStr = pThis->GetString();
        int thisStrLen = pThis->GetLength();

        charcount_t count = 0;

        if (args.Info.Count > 1)
        {
            if (!JavascriptOperators::IsUndefinedObject(args[1], scriptContext))
            {
                double countDbl = JavascriptConversion::ToInteger(args[1], scriptContext);
                if (JavascriptNumber::IsPosInf(countDbl) || countDbl < 0.0)
                {
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, L"String.prototype.repeat");
                }

                count = NumberUtilities::LuFromDblNearest(countDbl);
            }
        }

        if (count == 0 || thisStrLen == 0)
        {
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else if (count == 1)
        {
            return pThis;
        }

        charcount_t charCount = UInt32Math::Add(UInt32Math::Mul(count, thisStrLen), 1);
        wchar_t* buffer = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), wchar_t, charCount);

        if (thisStrLen == 1)
        {
            wmemset(buffer, thisStr[0], charCount - 1);
            buffer[charCount - 1] = '\0';
        }
        else
        {
            wchar_t* bufferDst = buffer;
            size_t bufferDstSize = charCount;

            for (charcount_t i = 0; i < count; i += 1)
            {
                js_wmemcpy_s(bufferDst, bufferDstSize, thisStr, thisStrLen);
                bufferDst += thisStrLen;
                bufferDstSize -= thisStrLen;
            }
            Assert(bufferDstSize == 1);
            *bufferDst = '\0';
        }

        return JavascriptString::NewWithBuffer(buffer, charCount - 1, scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// StartsWith() returns true if the given string matches the beginning of the
    /// substring starting at the given position in toString(this), as described
    /// in (ES6.0: S21.1.3.18).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryStartsWith(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(StartsWithCount);

        JavascriptString * pThis;
        JavascriptString * pSearch;

        GetThisAndSearchStringArguments(args, scriptContext, L"String.prototype.startsWith", &pThis, &pSearch, false);

        const wchar_t* thisStr = pThis->GetString();
        int thisStrLen = pThis->GetLength();

        const wchar_t* searchStr = pSearch->GetString();
        int searchStrLen = pSearch->GetLength();

        int startPosition = 0;

        if (args.Info.Count > 2)
        {
            if (!JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {
                startPosition = ConvertToIndex(args[2], scriptContext); // this is to adjust corner cases like MAX_VALUE
                startPosition = min(max(startPosition, 0), thisStrLen);
            }
        }

        // Avoid signed 32-bit int overflow if startPosition is large by subtracting searchStrLen from thisStrLen instead of
        // adding searchStrLen and startPosition.  The subtraction cannot underflow because maximum string length is
        // MaxCharCount == INT_MAX-1.  I.e. the RHS can be == 0 - (INT_MAX-1) == 1 - INT_MAX which would not underflow.
        if (startPosition <= thisStrLen - searchStrLen)
        {
            Assert(searchStrLen <= thisStrLen - startPosition);
            if (wmemcmp(thisStr + startPosition, searchStr, searchStrLen) == 0)
            {
                return scriptContext->GetLibrary()->GetTrue();
            }
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    ///----------------------------------------------------------------------------
    /// EndsWith() returns true if the given string matches the end of the
    /// substring ending at the given position in toString(this), as described
    /// in (ES6.0: S21.1.3.7).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryEndsWith(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(EndsWithCount);

        JavascriptString * pThis;
        JavascriptString * pSearch;

        GetThisAndSearchStringArguments(args, scriptContext, L"String.prototype.endsWith", &pThis, &pSearch, false);

        const wchar_t* thisStr = pThis->GetString();
        int thisStrLen = pThis->GetLength();

        const wchar_t* searchStr = pSearch->GetString();
        int searchStrLen = pSearch->GetLength();

        int endPosition = thisStrLen;

        if (args.Info.Count > 2)
        {
            if (!JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {
                endPosition = ConvertToIndex(args[2], scriptContext); // this is to adjust corner cases like MAX_VALUE
                endPosition = min(max(endPosition, 0), thisStrLen);
            }
        }

        int startPosition = endPosition - searchStrLen;

        if (startPosition >= 0)
        {
            Assert(startPosition <= thisStrLen);
            Assert(searchStrLen <= thisStrLen - startPosition);
            if (wmemcmp(thisStr + startPosition, searchStr, searchStrLen) == 0)
            {
                return scriptContext->GetLibrary()->GetTrue();
            }
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    ///----------------------------------------------------------------------------
    /// Includes() returns true if the given string matches any substring (of the
    /// same length) of the substring starting at the given position in
    /// toString(this), as described in (ES6.0 (draft 33): S21.1.3.7).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryIncludes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ContainsCount);

        return JavascriptBoolean::ToVar(IndexOf(args, scriptContext, L"String.prototype.includes", false) != -1, scriptContext);
    }

    Var JavascriptString::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if(args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.prototype.valueOf");
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString* str = nullptr;
        if (!GetThisValueVar(args[0], &str, scriptContext))
        {
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryValueOf, args, &result))
                {
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.prototype.valueOf");
        }

        return str;
    }

    Var JavascriptString::EntrySymbolIterator(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, L"String.prototype[Symbol.iterator]");
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        if (!JavascriptConversion::CheckObjectCoercible(args[0], scriptContext))
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, L"String.prototype[Symbol.iterator]");
        }

        JavascriptString* str = JavascriptConversion::ToString(args[0], scriptContext);

        return scriptContext->GetLibrary()->CreateStringIterator(str);
    }

    const wchar_t * JavascriptString::GetSz()
    {
        Assert(m_pszValue[m_charLength] == L'\0');
        return m_pszValue;
    }

    const wchar_t * JavascriptString::GetString()
    {
        if (!this->IsFinalized())
        {
            this->GetSz();
            Assert(m_pszValue);
        }
        return m_pszValue;
    }

    void const * JavascriptString::GetOriginalStringReference()
    {
        // Just return the string buffer
        return GetString();
    }

    size_t JavascriptString::GetAllocatedByteCount() const
    {
        if (!this->IsFinalized())
        {
            return 0;
        }
        return this->m_charLength * sizeof(WCHAR);
    }

    bool JavascriptString::IsSubstring() const
    {
        return false;
    }

    bool JavascriptString::IsNegZero(JavascriptString *string)
    {
        return string->GetLength() == 2 && wmemcmp(string->GetString(), L"-0", 2) == 0;
    }

    void JavascriptString::FinishCopy(__inout_xcount(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos)
    {
        while (!nestedStringTreeCopyInfos.IsEmpty())
        {
            const StringCopyInfo copyInfo(nestedStringTreeCopyInfos.Pop());
            Assert(copyInfo.SourceString()->GetLength() <= GetLength());
            Assert(copyInfo.DestinationBuffer() >= buffer);
            Assert(copyInfo.DestinationBuffer() <= buffer + (GetLength() - copyInfo.SourceString()->GetLength()));
            copyInfo.SourceString()->Copy(copyInfo.DestinationBuffer(), nestedStringTreeCopyInfos, 0);
        }
    }

    void JavascriptString::CopyVirtual(
        _Out_writes_(m_charLength) wchar_t *const buffer,
        StringCopyInfoStack &nestedStringTreeCopyInfos,
        const byte recursionDepth)
    {
        Assert(buffer);
        Assert(!this->IsFinalized());   // CopyVirtual should only be called for unfinalized buffers
        CopyHelper(buffer, GetString(), GetLength());
    }

    wchar_t* JavascriptString::GetSzCopy()
    {
        return AllocateLeafAndCopySz(this->GetScriptContext()->GetRecycler(), GetString(), GetLength());
    }

    LPCWSTR JavascriptString::GetSzCopy(ArenaAllocator* alloc)
    {
        return AllocateAndCopySz(alloc, GetString(), GetLength());
    }

    /*
    Table generated using the following:

    var invalidValue = 37;

    function toStringTable()
    {
        var stringTable = new Array(128);
        for(var i = 0; i < 128; i++)
        {
            var ch = i;
            if ('0'.charCodeAt(0) <= ch && '9'.charCodeAt(0) >= ch)
                ch -= '0'.charCodeAt(0);
            else if ('A'.charCodeAt(0)  <= ch && 'Z'.charCodeAt(0)  >= ch)
                ch -= 'A'.charCodeAt(0) - 10;
            else if ('a'.charCodeAt(0) <= ch && 'z'.charCodeAt(0) >= ch)
                ch -= 'a'.charCodeAt(0) - 10;
            else
                ch = 37;
            stringTable[i] = ch;
        }
        WScript.Echo("{" + stringTable + "}");
    }
    toStringTable();*/
    const char JavascriptString::stringToIntegerMap[] = {
        37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37
        ,37,37,37,37,37,37,37,37,0,1,2,3,4,5,6,7,8,9,37,37,37,37,37,37,37,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,
        28,29,30,31,32,33,34,35,37,37,37,37,37,37,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
        37,37,37,37,37};

    /*
    Table generated using the following:
    function logMaxUintTable()
    {
        var MAX_UINT = 4294967295;
        var logTable = new Array(37);
        logTable[0] = 0;
        logTable[1] = 0;
        for(var i = 2; i < logTable.length; i++)
        {
            logTable[i] = Math.floor(Math.log(MAX_UINT + 1) / Math.log(i));
        }
        WScript.Echo("{" + logTable + "}");
    }
    logMaxUintTable();
    */
    const uint8 JavascriptString::maxUintStringLengthTable[] =
        { 0,0,32,20,16,13,12,11,10,10,9,9,8,8,8,8,8,7,7,7,7,7,7,7,6,6,6,6,6,6,6,6,6,6,6,6,6 };

    // NumberUtil::FIntRadStrToDbl and parts of GlobalObject::EntryParseInt were refactored into ToInteger
    Var JavascriptString::ToInteger(int radix)
    {
        AssertMsg(radix == 0 || radix >= 2 && radix <= 36, "'radix' is invalid");
        const wchar_t* pchStart = GetString();
        const wchar_t* pchEnd =  pchStart + m_charLength;
        const wchar_t *pch = this->GetScriptContext()->GetCharClassifier()->SkipWhiteSpace(pchStart, pchEnd);
        bool isNegative = false;
        switch (*pch)
        {
        case '-':
            isNegative = true;
            // Fall through.
        case '+':
            if(pch < pchEnd)
            {
                pch++;
            }
            break;
        }

        if (0 == radix)
        {
            if (pch < pchEnd && '0' != pch[0])
            {
                radix = 10;
            }
            else if (('x' == pch[1] || 'X' == pch[1]) && pchEnd - pch >= 2)
            {
                radix = 16;
                pch += 2;
            }
            else
            {
                 // ES5's 'parseInt' does not allow treating a string beginning with a '0' as an octal value. ES3 does not specify a
                 // behavior
                 radix = 10;
            }
        }
        else if (16 == radix)
        {
            if('0' == pch[0] && ('x' == pch[1] || 'X' == pch[1]) && pchEnd - pch >= 2)
            {
                pch += 2;
            }
        }

        Assert(radix <= _countof(maxUintStringLengthTable));
        Assert(pchEnd >= pch);
        size_t length = pchEnd - pch;
        const wchar_t *const pchMin = pch;
        __analysis_assume(radix < _countof(maxUintStringLengthTable));
        if(length <= maxUintStringLengthTable[radix])
        {
            // Use uint32 as integer being parsed - much faster than BigInt
            uint32 value = 0;
            for ( ; pch < pchEnd ; pch++)
            {
                wchar_t ch = *pch;

                if(ch >= _countof(stringToIntegerMap) || (ch = stringToIntegerMap[ch]) >= radix)
                {
                    break;
                }
                uint32 beforeValue = value;
                value = value * radix + ch;
                AssertMsg(value >= beforeValue, "uint overflow");
            }

            if(pchMin == pch)
            {
                return GetScriptContext()->GetLibrary()->GetNaN();
            }

            if(isNegative)
            {
                // negative zero can only be represented by doubles
                if(value <= INT_MAX && value != 0)
                {
                    int32 result = -((int32)value);
                    return JavascriptNumber::ToVar(result, this->GetScriptContext());
                }
                double result = -((double)(value));
                return JavascriptNumber::New(result, this->GetScriptContext());
            }
            return JavascriptNumber::ToVar(value, this->GetScriptContext());
        }

        BigInt bi;
        for ( ; pch < pchEnd ; pch++)
        {
            wchar_t ch = *pch;

            if(ch >= _countof(stringToIntegerMap) || (ch = stringToIntegerMap[ch]) >= radix)
            {
                break;
            }
            if (!bi.FMulAdd(radix, ch))
            {
                //Mimic IE8 which threw a OutOfMemory exception in this case.
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
            // If we ever have more than 32 ulongs, the result must be infinite.
            if (bi.Clu() > 32)
            {
                Var result = isNegative ?
                    GetScriptContext()->GetLibrary()->GetNegativeInfinite() :
                    GetScriptContext()->GetLibrary()->GetPositiveInfinite();
                return result;
            }
        }

        if (pchMin == pch)
        {
            return GetScriptContext()->GetLibrary()->GetNaN();
        }

        // Convert to a double.
        double result = bi.GetDbl();
        if(isNegative)
        {
            result = -result;
        }

        return Js::JavascriptNumber::ToVarIntCheck(result, GetScriptContext());
    }

    bool JavascriptString::ToDouble(double * result)
    {

        const wchar_t* pch;
        long len = this->m_charLength;
        ScriptContext *scriptContext = this->GetScriptContext();
        if (0 == len)
        {
            *result = 0;
            return true;
        }

        if (1 == len && NumberUtilities::IsDigit(this->GetString()[0]))
        {
            *result = (double)(this->GetString()[0] - '0');
            return true;
        }

        // TODO: Use GetString here instead of GetSz (need to modify DblFromHex and StrToDbl to take a length)
        for (pch = this->GetSz(); IsWhiteSpaceCharacter(*pch); pch++)
            ;
        if (0 == *pch)
        {
            *result = 0;
            return true;
        }

        bool isNumericLiteral = false;
        if (*pch == '0')
        {
            const wchar_t *pchT = pch + 2;
            switch (pch[1])
            {
            case 'x':
            case 'X':

                *result = NumberUtilities::DblFromHex(pchT, &pch);
                isNumericLiteral = true;
                break;
            case 'o':
            case 'O':
                if (!scriptContext->GetConfig()->IsES6NumericLiteralEnabled())
                {
                    break;
                }
                *result = NumberUtilities::DblFromOctal(pchT, &pch);
                isNumericLiteral = true;
                break;
            case 'b':
            case 'B':
                if (!scriptContext->GetConfig()->IsES6NumericLiteralEnabled())
                {
                    break;
                }
                *result = NumberUtilities::DblFromBinary(pchT, &pch);
                isNumericLiteral = true;
                break;
            }
            if (pchT == pch && isNumericLiteral)
            {
                *result = JavascriptNumber::NaN;
                return false;
            }
        }
        if (!isNumericLiteral)
        {
            *result = NumberUtilities::StrToDbl(pch, &pch, GetScriptContext());
        }

        while (IsWhiteSpaceCharacter(*pch))
            pch++;
        if (pch != this->m_pszValue + len)
        {
            *result = JavascriptNumber::NaN;
            return false;
        }
        return true;
    }

    double JavascriptString::ToDouble()
    {
        double result;
        this->ToDouble(&result);
        return result;
    }

    bool JavascriptString::Equals(Var aLeft, Var aRight)
    {
        AssertMsg(JavascriptString::Is(aLeft) && JavascriptString::Is(aRight), "string comparison");

        JavascriptString *leftString  = JavascriptString::FromVar(aLeft);
        JavascriptString *rightString = JavascriptString::FromVar(aRight);

        if (leftString->GetLength() != rightString->GetLength())
        {
            return false;
        }

        if (wmemcmp(leftString->GetString(), rightString->GetString(), leftString->GetLength()) == 0)
        {
            return true;
        }
        return false;
    }

    //
    // LessThan implements algorithm of ES5 11.8.5 step 4
    // returns false for same string pattern
    //
    bool JavascriptString::LessThan(Var aLeft, Var aRight)
    {
        AssertMsg(JavascriptString::Is(aLeft) && JavascriptString::Is(aRight), "string LessThan");

        JavascriptString *leftString  = JavascriptString::FromVar(aLeft);
        JavascriptString *rightString = JavascriptString::FromVar(aRight);

        if (JavascriptString::strcmp(leftString, rightString) < 0)
        {
            return true;
        }
        return false;
    }

    // thisStringValue(value) abstract operation as defined in ES6.0 (Draft 25) Section 21.1.3
    BOOL JavascriptString::GetThisValueVar(Var aValue, JavascriptString** pString, ScriptContext* scriptContext)
    {
        Assert(pString);

        // 1. If Type(value) is String, return value.
        if (JavascriptString::Is(aValue))
        {
            *pString = JavascriptString::FromVar(aValue);
            return TRUE;
        }
        // 2. If Type(value) is Object and value has a [[StringData]] internal slot
        else if ( JavascriptStringObject::Is(aValue))
        {
            JavascriptStringObject* pStringObj = JavascriptStringObject::FromVar(aValue);

            // a. Let s be the value of value's [[StringData]] internal slot.
            // b. If s is not undefined, then return s.
            *pString = JavascriptString::FromVar(CrossSite::MarshalVar(scriptContext, pStringObj->Unwrap()));
            return TRUE;
        }

        // 3. Throw a TypeError exception.
        // Note: We don't throw a TypeError here, choosing to return FALSE and let the caller throw the error
        return FALSE;
    }

#ifdef TAGENTRY
#undef TAGENTRY
#endif
#define TAGENTRY(name, ...) \
        Var JavascriptString::Entry##name(RecyclableObject* function, CallInfo callInfo, ...)   \
        {                                                                                       \
            PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);          \
                                                                                                \
            ARGUMENTS(args, callInfo);                                                          \
            ScriptContext* scriptContext = function->GetScriptContext();                        \
                                                                                                \
            Assert(!(callInfo.Flags & CallFlags_New));                                          \
                                                                                                \
            return StringBracketHelper(args, scriptContext, __VA_ARGS__);                       \
        }
#include "JavascriptStringTagEntries.h"
#undef TAGENTRY

    Var JavascriptString::StringBracketHelper(Arguments args, ScriptContext *scriptContext, __in_ecount(cchTag) wchar_t const *pszTag,
                                                charcount_t cchTag, __in_ecount_opt(cchProp) wchar_t const *pszProp, charcount_t cchProp)
    {
        charcount_t cchThis;
        charcount_t cchPropertyValue;
        charcount_t cchTotalChars;
        charcount_t ich;
        JavascriptString * pThis;
        JavascriptString * pPropertyValue = nullptr;
        const wchar_t * propertyValueStr = nullptr;
        uint quotesCount = 0;
        const wchar_t quotStr[] = L"&quot;";
        const charcount_t quotStrLen = _countof(quotStr) - 1;
        bool ES6FixesEnabled = scriptContext->GetConfig()->IsES6StringPrototypeFixEnabled();

        // Assemble the component pieces of a string tag function (ex: String.prototype.link).
        // In the general case, result is as below:
        //
        // pszProp = L"href";
        // pszTag = L"a";
        // pThis = JavascriptString::FromVar(args[0]);
        // pPropertyValue = JavascriptString::FromVar(args[1]);
        //
        // pResult = L"<a href=\"[[pPropertyValue]]\">[[pThis]]</a>";
        //
        // cchTotalChars = 5                    // <></>
        //                 + cchTag * 2         // a
        //                 + cchProp            // href
        //                 + 4                  // _=""
        //                 + cchPropertyValue
        //                 + cchThis;
        //
        // Note: With ES6FixesEnabled, we need to escape quote characters (L'"') in pPropertyValue.
        // Note: Without ES6FixesEnabled, the tag and prop strings should be capitalized.

        if(args.Info.Count == 0)
        {
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedString);
        }

        if (ES6FixesEnabled)
        {
            if (!JavascriptConversion::CheckObjectCoercible(args[0], scriptContext))
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, pszTag);
            }
        }

        if (JavascriptString::Is(args[0]))
        {
            pThis = JavascriptString::FromVar(args[0]);
        }
        else
        {
            pThis = JavascriptConversion::ToString(args[0], scriptContext);
        }

        cchThis = pThis->GetLength();
        cchTotalChars = UInt32Math::Add(cchTag, cchTag);

        // 5 is for the <></> characters
        cchTotalChars = UInt32Math::Add(cchTotalChars, 5);

        if (nullptr != pszProp)
        {
            // Need one string argument.
            if (args.Info.Count >= 2)
            {
                if (JavascriptString::Is(args[1]))
                {
                    pPropertyValue = JavascriptString::FromVar(args[1]);
                }
                else
                {
                    pPropertyValue = JavascriptConversion::ToString(args[1], scriptContext);
                }
            }
            else
            {
                pPropertyValue = scriptContext->GetLibrary()->GetUndefinedDisplayString();
            }

            cchPropertyValue = pPropertyValue->GetLength();
            propertyValueStr = pPropertyValue->GetString();

            if (ES6FixesEnabled)
            {
                // Count the number of " characters we need to escape.
                for (ich = 0; ich < cchPropertyValue; ich++)
                {
                    if (propertyValueStr[ich] == L'"')
                    {
                        ++quotesCount;
                    }
                }
            }

            cchTotalChars = UInt32Math::Add(cchTotalChars, cchProp);

            // 4 is for the _="" characters
            cchTotalChars = UInt32Math::Add(cchTotalChars, 4);

            if (ES6FixesEnabled)
            {
                // Account for the " escaping (&quot;)
                cchTotalChars = UInt32Math::Add(cchTotalChars, UInt32Math::Mul(quotesCount, quotStrLen)) - quotesCount;
            }
        }
        else
        {
            cchPropertyValue = 0;
            cchProp = 0;
        }
        cchTotalChars = UInt32Math::Add(cchTotalChars, cchThis);
        cchTotalChars = UInt32Math::Add(cchTotalChars, cchPropertyValue);
        if (!IsValidCharCount(cchTotalChars) || cchTotalChars < cchThis || cchTotalChars < cchPropertyValue)
        {
            Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
        }

        BufferStringBuilder builder(cchTotalChars, scriptContext);
        wchar_t *pResult = builder.DangerousGetWritableBuffer();

        *pResult++ = L'<';
        for (ich = 0; ich < cchTag; ich++)
        {
            *pResult++ = ES6FixesEnabled ? pszTag[ich] : towupper(pszTag[ich]);
        }
        if (nullptr != pszProp)
        {
            *pResult++ = L' ';
            for (ich = 0; ich < cchProp; ich++)
            {
                *pResult++ = ES6FixesEnabled ? pszProp[ich] : towupper(pszProp[ich]);
            }
            *pResult++ = L'=';
            *pResult++ = L'"';

            Assert(propertyValueStr != nullptr);

            if (!ES6FixesEnabled || quotesCount == 0)
            {
                js_wmemcpy_s(pResult,
                    cchTotalChars - (pResult - builder.DangerousGetWritableBuffer() + 1),
                    propertyValueStr,
                    cchPropertyValue);

                pResult += cchPropertyValue;
            }
            else {
                for (ich = 0; ich < cchPropertyValue; ich++)
                {
                    if (propertyValueStr[ich] == L'"')
                    {
                        charcount_t destLengthLeft = (cchTotalChars - (charcount_t)(pResult - builder.DangerousGetWritableBuffer() + 1));

                        // Copy the quote string into result beginning at the index where the quote would appear
                        js_wmemcpy_s(pResult,
                            destLengthLeft,
                            quotStr,
                            quotStrLen);

                        // Move result ahead by the length of the quote string
                        pResult += quotStrLen;
                        // We ate one of the quotes
                        quotesCount--;

                        // We only need to check to see if we have no more quotes after eating a quote
                        if (quotesCount == 0)
                        {
                            // Skip the quote character.
                            // Note: If ich is currently the last character (cchPropertyValue-1), it becomes cchPropertyValue after incrementing.
                            // At that point, cchPropertyValue - ich == 0 so we will not increment pResult and will call memcpy for zero bytes.
                            ich++;

                            // Copy the rest from the property value string starting at the index after the last quote
                            js_wmemcpy_s(pResult,
                                destLengthLeft - quotStrLen,
                                propertyValueStr + ich,
                                cchPropertyValue - ich);

                            // Move result ahead by the length of the rest of the property string
                            pResult += (cchPropertyValue - ich);
                            break;
                        }
                    }
                    else
                    {
                        // Each non-quote character just gets copied into result string
                        *pResult++ = propertyValueStr[ich];
                    }
                }
            }

            *pResult++ = L'"';
        }
        *pResult++ = L'>';

        const wchar_t *pThisString = pThis->GetString();
        js_wmemcpy_s(pResult, cchTotalChars - (pResult - builder.DangerousGetWritableBuffer() + 1), pThisString, cchThis);
        pResult += cchThis;

        *pResult++ = L'<';
        *pResult++ = L'/';
        for (ich = 0; ich < cchTag; ich++)
        {
            *pResult++ = ES6FixesEnabled ? pszTag[ich] : towupper(pszTag[ich]);
        }
        *pResult++ = L'>';

        // Assert we ended at the right place.
        AssertMsg((charcount_t)(pResult - builder.DangerousGetWritableBuffer()) == cchTotalChars, "Exceeded allocated string limit");

        return builder.ToString();
    }
    Var JavascriptString::ToLocaleCaseHelper(Var thisObj, bool toUpper, ScriptContext *scriptContext)
    {
        JavascriptString * pThis;

        if (JavascriptString::Is(thisObj))
        {
            pThis = JavascriptString::FromVar(thisObj);
        }
        else
        {
            pThis = JavascriptConversion::ToString(thisObj, scriptContext);
        }

        if (pThis->GetLength() == 0)
        {
            return pThis;
        }

        DWORD dwFlags = toUpper ? LCMAP_UPPERCASE : LCMAP_LOWERCASE;
        dwFlags |= LCMAP_LINGUISTIC_CASING;

        LCID lcid = GetUserDefaultLCID();

        const wchar_t* str = pThis->GetString();

        // Get the number of chars in the mapped string.
        int count = LCMapStringW(lcid, dwFlags, str, pThis->GetLength(), NULL, 0);

        if (0 == count)
        {
            AssertMsg(FALSE, "LCMapString failed");
            Throw::InternalError();
        }

        BufferStringBuilder builder(count, scriptContext);
        wchar_t * stringBuffer = builder.DangerousGetWritableBuffer();

        int count1 = LCMapStringW(lcid, dwFlags, str, count, stringBuffer, count);

        if( 0 == count1 )
        {
            AssertMsg(FALSE, "LCMapString failed");
            Throw::InternalError();
        }

        return builder.ToString();
    }

    int JavascriptString::IndexOfUsingJmpTable(JmpTable jmpTable, const wchar_t* inputStr, int len, const wchar_t* searchStr, int searchLen, int position)
    {
        int result = -1;

        const wchar_t searchLast = searchStr[searchLen-1];

        unsigned long lMatchedJump = searchLen;
        if (jmpTable[searchLast].shift > 0)
        {
            lMatchedJump = jmpTable[searchLast].shift;
        }

        wchar_t const * p = inputStr + position + searchLen-1;
        WCHAR c;
        while(p < inputStr + len)
        {
            // first character match, keep checking
            if (*p == searchLast)
            {
                if ( wmemcmp(p-searchLen+1, searchStr, searchLen) == 0 )
                {
                    break;
                }
                p += lMatchedJump;
            }
            else
            {
                c = *p;
                if ( 0 == ( c & ~0x7f ) && jmpTable[c].shift != 0 )
                {
                    p += jmpTable[c].shift;
                }
                else
                {
                    p += searchLen;
                }
            }
        }

        if (p >= inputStr+position && p < inputStr + len)
        {
            result = (int)(p - inputStr) - searchLen + 1;
        }

        return result;
    }

    int JavascriptString::LastIndexOfUsingJmpTable(JmpTable jmpTable, const wchar_t* inputStr, int len, const wchar_t* searchStr, int searchLen, int position)
    {
        const wchar_t searchFirst = searchStr[0];
        unsigned long lMatchedJump = searchLen;
        if (jmpTable[searchFirst].shift > 0)
        {
            lMatchedJump = jmpTable[searchFirst].shift;
        }
        WCHAR c;
        wchar_t const * p = inputStr + min(len - searchLen, position);
        while(p >= inputStr)
        {
            // first character match, keep checking
            if (*p == searchFirst)
            {
                if ( wmemcmp(p, searchStr, searchLen) == 0 )
                {
                    break;
                }
                p -= lMatchedJump;
            }
            else
            {
                c = *p;
                if ( 0 == ( c & ~0x7f ) && jmpTable[c].shift != 0 )
                {
                    p -= jmpTable[c].shift;
                }
                else
                {
                    p -= searchLen;
                }
            }
        }
        return ((p >= inputStr) ? (int)(p - inputStr) : -1);
    }

    bool JavascriptString::BuildLastCharForwardBoyerMooreTable(JmpTable jmpTable, const wchar_t* searchStr, int searchLen)
    {
        AssertMsg(searchLen >= 1, "Table for non-empty string");
        memset(jmpTable, 0, sizeof(JmpTable));

        const wchar_t * p2 = searchStr + searchLen - 1;
        const wchar_t * const begin = searchStr;

        // Determine if we can do an partial ASCII Boyer-Moore
        while (p2 >= begin)
        {
            WCHAR c = *p2;
            if ( 0 == ( c & ~0x7f ))
            {
                if ( jmpTable[c].shift == 0 )
                {
                    jmpTable[c].shift = (unsigned long)(searchStr + searchLen - 1 - p2);
                }
            }
            else
            {
                return false;
            }
            p2--;
        }

        return true;
    }

    bool JavascriptString::BuildFirstCharBackwardBoyerMooreTable(JmpTable jmpTable, const wchar_t* searchStr, int searchLen)
    {
        AssertMsg(searchLen >= 1, "Table for non-empty string");
        memset(jmpTable, 0, sizeof(JmpTable));

        const wchar_t * p2 = searchStr;
        const wchar_t * const end = searchStr + searchLen;

        // Determine if we can do an partial ASCII Boyer-Moore
        while (p2 < end)
        {
            WCHAR c = *p2;
            if ( 0 == ( c & ~0x7f ))
            {
                if ( jmpTable[c].shift == 0 )
                {
                    jmpTable[c].shift = (unsigned long)(p2 - searchStr);
                }
            }
            else
            {
                return false;
            }
            p2++;
        }

        return true;
    }

    uint JavascriptString::strstr(JavascriptString *string, JavascriptString *substring, bool useBoyerMoore, uint start)
    {
        uint i;

        const wchar_t *stringOrig = string->GetString();
        uint stringLenOrig = string->GetLength();
        const wchar_t *stringSz = stringOrig + start;
        const wchar_t *substringSz = substring->GetString();
        uint stringLen = stringLenOrig - start;
        uint substringLen = substring->GetLength();

        if (useBoyerMoore && substringLen > 2)
        {
            JmpTable jmpTable;
            bool fAsciiJumpTable = BuildLastCharForwardBoyerMooreTable(jmpTable, substringSz, substringLen);
            if (fAsciiJumpTable)
            {
                int result = IndexOfUsingJmpTable(jmpTable, stringOrig, stringLenOrig, substringSz, substringLen, start);
                if (result != -1)
                {
                    return result;
                }
                else
                {
                    return (uint)-1;
                }
            }
        }

        if (stringLen >= substringLen)
        {
            // If substring is empty, it matches anything...
            if (substringLen == 0)
            {
                return 0;
            }
            for (i = 0; i <= stringLen - substringLen; i++)
            {
                // Quick check for first character.
                if (stringSz[i] == substringSz[0])
                {
                    if (substringLen == 1 || memcmp(stringSz+i+1, substringSz+1, (substringLen-1)*sizeof(wchar_t)) == 0)
                    {
                        return i + start;
                    }
                }
            }
        }

        return (uint)-1;
    }

    int JavascriptString::strcmp(JavascriptString *string1, JavascriptString *string2)
    {
        uint string1Len = string1->GetLength();
        uint string2Len = string2->GetLength();

        int result = wmemcmp(string1->GetString(), string2->GetString(), min(string1Len, string2Len));

        return (result == 0) ? (int)(string1Len - string2Len) : result;
    }

    /*static*/ charcount_t JavascriptString::SafeSzSize(charcount_t cch)
    {
        // JavascriptString::MaxCharLength is valid; however, we are incrementing below by 1 and want to make sure we aren't overflowing
        // Nor going outside of valid range.
        if (cch >= JavascriptString::MaxCharLength)
        {
            Throw::OutOfMemory();
        }

        // Compute cch + 1, checking for overflow
        ++cch;

        return cch;
    }

    charcount_t JavascriptString::SafeSzSize() const
    {
        return SafeSzSize(GetLength());
    }

    /*static*/ __ecount(length+1) wchar_t* JavascriptString::AllocateLeafAndCopySz(__in Recycler* recycler, __in_ecount(length) const wchar_t* content, charcount_t length)
    {
        // Note: Intentionally not using SafeSzSize nor hoisting common
        // sub-expression "length + 1" into a local variable otherwise
        // Prefast gets confused and cannot track buffer's length.

        // JavascriptString::MaxCharLength is valid; however, we are incrementing below by 1 and want to make sure we aren't overflowing
        // Nor going outside of valid range.
        if (length >= JavascriptString::MaxCharLength)
        {
            Throw::OutOfMemory();
        }

        charcount_t bufLen = length + 1;
        // Allocate recycler memory to store the string plus a terminating NUL
        wchar_t* buffer = RecyclerNewArrayLeaf(recycler, wchar_t, bufLen);
        js_wmemcpy_s(buffer, bufLen, content, length);
        buffer[length] = L'\0';

        return buffer;
    }

    /*static*/ __ecount(length+1) wchar_t* JavascriptString::AllocateAndCopySz(__in ArenaAllocator* arena, __in_ecount(length) const wchar_t* content, charcount_t length)
    {
        // Note: Intentionally not using SafeSzSize nor hoisting common
        // sub-expression "length + 1" into a local variable otherwise
        // Prefast gets confused and cannot track buffer's length.

        // JavascriptString::MaxCharLength is valid; however, we are incrementing below by 1 and want to make sure we aren't overflowing
        // Nor going outside of valid range.
        if (length >= JavascriptString::MaxCharLength)
        {
            Throw::OutOfMemory();
        }

        // Allocate arena memory to store the string plus a terminating NUL
        wchar_t* buffer = AnewArray(arena, wchar_t, length + 1);
        js_wmemcpy_s(buffer, length + 1, content, length);
        buffer[length] = L'\0';

        return buffer;
    }

    RecyclableObject * JavascriptString::CloneToScriptContext(ScriptContext* requestContext)
    {
        return JavascriptString::NewWithBuffer(this->GetSz(), this->GetLength(), requestContext);
    }

    charcount_t JavascriptString::ConvertToIndex(Var varIndex, ScriptContext *scriptContext)
    {
        if (TaggedInt::Is(varIndex))
        {
            return TaggedInt::ToInt32(varIndex);
        }
        return NumberUtilities::LwFromDblNearest(JavascriptConversion::ToInteger(varIndex, scriptContext));
    }

    wchar_t* JavascriptString::GetNormalizedString(_NORM_FORM form, ArenaAllocator* tempAllocator, charcount_t& sizeOfNormalizedStringWithoutNullTerminator)
    {
        ScriptContext* scriptContext = this->GetScriptContext();
        if (this->GetLength() == 0)
        {
            sizeOfNormalizedStringWithoutNullTerminator = 0;
            return nullptr;
        }

        // IMPORTANT: Implementation Notes
        // Normalize string estimates the required size of the buffer based on averages and other data.
        // It is very hard to get a precise size from an input string without expanding/contracting it on the buffer.
        // It is estimated that the maximum size the string after an NFC is 6x the input length, and 18x for NFD. This approach isn't very feasible as well.
        // The approach taken is based on the simple example in the MSDN article.
        //  - Loop until the return value is either an error (apart from insufficient buffer size), or success.
        //  - Each time recreate a temporary buffer based on the last guess.
        //  - When creating the JS string, use the positive return value and copy the buffer across.
        // Design choice for "guesses" comes from data Windows collected; and in most cases the loop will not iterate more than 2 times.

        Assert(!IsNormalizedString(form, this->GetSz(), this->GetLength()));

        //Get the first size estimate
        int sizeEstimate = NormalizeString(form, this->GetSz(), this->GetLength() + 1, nullptr, 0);
        wchar_t *tmpBuffer = nullptr;
        DWORD lastError = ERROR_INSUFFICIENT_BUFFER;
        //Loop while the size estimate is bigger than 0
        while (lastError == ERROR_INSUFFICIENT_BUFFER)
        {
            tmpBuffer = AnewArray(tempAllocator, wchar_t, sizeEstimate);
            sizeEstimate = NormalizeString(form, this->GetSz(), this->GetLength() + 1, tmpBuffer, sizeEstimate);

            // Success, sizeEstimate is the exact size including the null terminator
            if (sizeEstimate > 0)
            {
                sizeOfNormalizedStringWithoutNullTerminator = sizeEstimate - 1;
                return tmpBuffer;
            }

            //Anything less than 0, we have an error, flip sizeEstimate now. As both times we need to use it, we need positive anyways.
            lastError = GetLastError();
            sizeEstimate *= -1;

        }

        switch ((long)lastError)
        {
        case ERROR_INVALID_PARAMETER:
            //some invalid parameter, coding error
            AssertMsg(false, "ERROR_INVALID_PARAMETER check pointers passed to NormalizeString");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FailedToNormalize);
            break;
        case ERROR_NO_UNICODE_TRANSLATION:
            //the value returned is the negative index of an invalid unicode character
            JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidUnicodeCharacter, sizeEstimate);
            break;
        case ERROR_SUCCESS:
            //The actual size of the output string is zero.
            //Theoretically only empty input string should produce this, which is handled above, thus the code path should not be hit.
            AssertMsg(false, "This code path should not be hit, empty string case is handled above. Perhaps a false error (sizeEstimate <= 0; but lastError == 0; ERROR_SUCCESS and NO_ERRROR == 0)");
            sizeOfNormalizedStringWithoutNullTerminator = 0;
            return nullptr; // scriptContext->GetLibrary()->GetEmptyString();

            break;
        default:
            AssertMsg(false, "Unknown error. MSDN documentation didn't specify any additional errors.");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FailedToNormalize);
            break;
        }
    }

    void JavascriptString::InstantiateForceInlinedMembers()
    {
        // Force-inlined functions defined in a translation unit need a reference from an extern non-force-inlined function in
        // the same translation unit to force an instantiation of the force-inlined function. Otherwise, if the force-inlined
        // function is not referenced in the same translation unit, it will not be generated and the linker is not able to find
        // the definition to inline the function in other translation units.
        Assert(false);

        JavascriptString *const s = nullptr;

        s->ConcatDestructive(nullptr);
    }

    JavascriptString *
    JavascriptString::Concat3(JavascriptString * pstLeft, JavascriptString * pstCenter, JavascriptString * pstRight)
    {
        ConcatStringMulti * concatString = ConcatStringMulti::New(3, pstLeft, pstCenter, pstLeft->GetScriptContext());
        concatString->SetItem(2, pstRight);
        return concatString;
    }

    BOOL JavascriptString::HasProperty(PropertyId propertyId)
    {
        if (propertyId == PropertyIds::length)
        {
            return true;
        }
        ScriptContext* scriptContext = GetScriptContext();
        charcount_t index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            if (index < this->GetLength())
            {
                return true;
            }
        }
        return false;
    }

    BOOL JavascriptString::IsEnumerable(PropertyId propertyId)
    {
        ScriptContext* scriptContext = GetScriptContext();
        charcount_t index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {
            if (index < this->GetLength())
            {
                return true;
            }
        }
        return false;
    }

    BOOL JavascriptString::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return GetPropertyBuiltIns(propertyId, value);
    }
    BOOL JavascriptString::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value))
        {
            return true;
        }
        return false;
    }
    bool JavascriptString::GetPropertyBuiltIns(PropertyId propertyId, Var* value)
    {
        if (propertyId == PropertyIds::length)
        {
            *value = JavascriptNumber::ToVar(this->GetLength(), this->GetScriptContext());
            return true;
        }

        return false;
    }
    BOOL JavascriptString::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        return JavascriptString::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptString::SetItem(uint32 index, Var value, PropertyOperationFlags propertyOperationFlags)
    {
        if (this->HasItemAt(index))
        {
            JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, this->GetScriptContext());

            return FALSE;
        }

        return __super::SetItem(index, value, propertyOperationFlags);
    }

    BOOL JavascriptString::DeleteItem(uint32 index, PropertyOperationFlags propertyOperationFlags)
    {
        if (this->HasItemAt(index))
        {
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), TaggedInt::ToString(index, this->GetScriptContext())->GetString());

            return FALSE;
        }

        return __super::DeleteItem(index, propertyOperationFlags);
    }

    BOOL JavascriptString::HasItem(uint32 index)
    {
        return this->HasItemAt(index);
    }

    BOOL JavascriptString::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        // String should always be marshalled to the current context
        Assert(requestContext == this->GetScriptContext());
        return this->GetItemAt(index, value);
    }

    BOOL JavascriptString::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {
        // String should always be marshalled to the current context
        return this->GetItemAt(index, value);
    }

    BOOL JavascriptString::GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext * requestContext, bool preferSnapshotSemantics, bool enumSymbols)
    {
        *enumerator = RecyclerNew(GetScriptContext()->GetRecycler(), JavascriptStringEnumerator, this, requestContext);
        return true;
    }

    BOOL JavascriptString::DeleteProperty(PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {
        if (propertyId == PropertyIds::length)
        {
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), this->GetScriptContext()->GetPropertyName(propertyId)->GetBuffer());

            return FALSE;
        }
        return __super::DeleteProperty(propertyId, propertyOperationFlags);
    }

    BOOL JavascriptString::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->Append(L'"');
        stringBuilder->Append(this->GetString(), this->GetLength());
        stringBuilder->Append(L'"');
        return TRUE;
    }

    BOOL JavascriptString::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        stringBuilder->AppendCppLiteral(L"String");
        return TRUE;
    }

    RecyclableObject* JavascriptString::ToObject(ScriptContext * requestContext)
    {
        return requestContext->GetLibrary()->CreateStringObject(this);
    }

    Var JavascriptString::GetTypeOfString(ScriptContext * requestContext)
    {
        return requestContext->GetLibrary()->GetStringTypeDisplayString();
    }
}
