//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    typedef struct
    {
        unsigned long shift;
    } Boyer_Moore_Jump;

    // Boyer Moore table for only the first character in the search string.
#if defined(_M_IX86) || defined(_M_IX64)
    // This table gets memset to 0. The x86 CRT is optimized for doing copies 128 bytes
    // at a time, for 16 byte aligned memory. If this table isn't 16 byte aligned, we pay
    // an additional cost to pre-align it, dealing with trailing bytes, and the copy
    // ends up being twice as slow.
    typedef Boyer_Moore_Jump __declspec(align(16)) JmpTable[0x80];
#else
    typedef Boyer_Moore_Jump JmpTable[0x80];
#endif

    struct PropertyCache;
    class SubString;
    class StringCopyInfoStack;

    bool IsValidCharCount(size_t charCount);
    const charcount_t k_InvalidCharCount = static_cast<charcount_t>(-1);


    //
    // To inspect strings in hybrid debugging, we use vtable lookup to find out concrete string type
    // then inspect string content accordingly.
    //
    // To ensure all known string vtables are listed and exported from chakra.dll and handler class
    // exists in chakradiag.dll, declare an abstract method in base JavascriptString class. Any concrete
    // subclass that has runtime string instance must DECLARE_CONCRETE_STRING_CLASS, otherwise
    // we'll get a compile time error.
    //
#if DBG && defined(NTBUILD)
#define DECLARE_CONCRETE_STRING_CLASS_BASE  virtual void _declareConcreteStringClass() = 0
#define DECLARE_CONCRETE_STRING_CLASS       virtual void _declareConcreteStringClass() override
#else
#define DECLARE_CONCRETE_STRING_CLASS_BASE
#define DECLARE_CONCRETE_STRING_CLASS
#endif

    class JavascriptString abstract : public RecyclableObject
    {
        friend Lowerer;
        friend LowererMD;
        friend bool IsValidCharCount(size_t);

    private:
        const wchar_t* m_pszValue;         // Flattened, '\0' terminated contents
        charcount_t m_charLength;          // Length in characters, not including '\0'.

        static const charcount_t MaxCharLength = INT_MAX - 1;  // Max number of chars not including '\0'.

    protected:
        static const byte MaxCopyRecursionDepth = 3;

    public:

        BOOL HasItemAt(charcount_t idxChar);
        BOOL GetItemAt(charcount_t idxChar, Var* value);
        wchar_t GetItem(charcount_t index);

        _Ret_range_(m_charLength, m_charLength) charcount_t GetLength() const;
        virtual size_t GetAllocatedByteCount() const;
        virtual bool IsSubstring() const;
        int GetLengthAsSignedInt() const;
        const wchar_t* UnsafeGetBuffer() const;
        LPCWSTR GetSzCopy(ArenaAllocator* alloc);   // Copy to an Arena
        const wchar_t* GetString(); // Get string, may not be NULL terminated

        // NumberUtil::FIntRadStrToDbl and parts of GlobalObject::EntryParseInt were refactored into ToInteger
        Var ToInteger(int radix = 0);

        double ToDouble();
        bool ToDouble(double * result);

        static const wchar_t* GetSzHelper(JavascriptString *str) { return str->GetSz(); }
        virtual const wchar_t* GetSz();     // Get string, NULL terminated
        virtual void const * GetOriginalStringReference();  // Get the original full string (Same as GetString() unless it is a SubString);

    public:
        template <typename StringType>
        void Copy(__out_ecount(bufLen) wchar_t *const buffer, const charcount_t bufLen);
        void Copy(__out_xcount(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth)
        {
            if (this->IsFinalized())
            {
                // If we have the buffer already, just copy it
                const CharCount copyCharLength = this->GetLength();
                CopyHelper(buffer, this->GetString(), copyCharLength);
            }
            else
            {
                CopyVirtual(buffer, nestedStringTreeCopyInfos, recursionDepth);
            }
        }
        virtual void CopyVirtual(_Out_writes_(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth);

    private:
        void FinishCopy(__inout_xcount(m_charLength) wchar_t *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos);

    public:
        virtual int GetRandomAccessItemsFromConcatString(Js::JavascriptString * const *& items) const { return -1; }
        virtual bool IsTree() const { return false; }

        virtual BOOL SetItem(uint32 index, Var value, PropertyOperationFlags propertyOperationFlags) override;
        virtual BOOL DeleteItem(uint32 index, PropertyOperationFlags propertyOperationFlags) override;
        virtual BOOL HasItem(uint32 index) override sealed;
        virtual BOOL GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext) override;
        virtual BOOL GetEnumerator(BOOL enumNonEnumerable, Var* enumerator, ScriptContext * requestContext, bool preferSnapshotSemantics = true, bool enumSymbols = false) override;
        virtual BOOL HasProperty(PropertyId propertyId) override;
        virtual BOOL IsEnumerable(PropertyId propertyId) override;
        virtual BOOL DeleteProperty(PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)  override;
        virtual BOOL GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext) override;
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual RecyclableObject* ToObject(ScriptContext * requestContext) override;
        virtual Var GetTypeOfString(ScriptContext * requestContext) override;
        // should never be called, JavascriptConversion::ToPrimitive() short-circuits and returns input value
        virtual BOOL ToPrimitive(JavascriptHint hint, Var* value, ScriptContext * requestContext) override { AssertMsg(false, "String ToPrimitive should not be called"); *value = this; return true;}
        virtual RecyclableObject * CloneToScriptContext(ScriptContext* requestContext) override;

        virtual BOOL BufferEquals(__in_ecount(otherLength) LPCWSTR otherBuffer, __in charcount_t otherLength);
        virtual wchar_t* GetNormalizedString(NORM_FORM, ArenaAllocator*, charcount_t&);

        static bool Is(Var aValue);
        static JavascriptString* FromVar(Var aValue);
        static bool Equals(Var aLeft, Var aRight);
        static bool LessThan(Var aLeft, Var aRight);
        static bool IsNegZero(JavascriptString *string);

        static uint strstr(JavascriptString *string, JavascriptString *substring, bool useBoyerMoore, uint start=0);
        static int strcmp(JavascriptString *string1, JavascriptString *string2);

    private:
        enum ToCase{
            ToLower,
            ToUpper
        };
        wchar_t* GetSzCopy();   // get a copy of the inner string without compacting the chunks

        static Var ToCaseCore(JavascriptString* pThis, ToCase toCase);
        static int IndexOfUsingJmpTable(JmpTable jmpTable, const wchar_t* inputStr, int len, const wchar_t* searchStr, int searchLen, int position);
        static int LastIndexOfUsingJmpTable(JmpTable jmpTable, const wchar_t* inputStr, int len, const wchar_t* searchStr, int searchLen, int position);
        static bool BuildLastCharForwardBoyerMooreTable(JmpTable jmpTable, const wchar_t* searchStr, int searchLen);
        static bool BuildFirstCharBackwardBoyerMooreTable(JmpTable jmpTable, const wchar_t* searchStr, int searchLen);
        static charcount_t ConvertToIndex(Var varIndex, ScriptContext *scriptContext);

        template <typename T, bool copyBuffer>
        static JavascriptString* NewWithBufferT(const wchar_t * content, charcount_t charLength, ScriptContext * scriptContext);

        bool GetPropertyBuiltIns(PropertyId propertyId, Var* value);
        static const char stringToIntegerMap[128];
        static const uint8 maxUintStringLengthTable[37];
    protected:
        JavascriptString(StaticType * type);
        JavascriptString(StaticType * type, charcount_t charLength, const wchar_t* szValue);
        DEFINE_VTABLE_CTOR_ABSTRACT(JavascriptString, RecyclableObject);
        DECLARE_CONCRETE_STRING_CLASS_BASE;

        void SetLength(charcount_t newLength);
        void SetBuffer(const wchar_t* buffer);
        bool IsValidIndexValue(charcount_t idx) const;

        static charcount_t SafeSzSize(charcount_t length); // Throws on overflow
        charcount_t SafeSzSize() const; // Throws on overflow

    public:
        bool IsFinalized() const { return this->UnsafeGetBuffer() != NULL; }

    public:
        static JavascriptString* NewWithSz(__in_z const wchar_t * content, ScriptContext* scriptContext);
        static JavascriptString* NewWithBuffer(__in_ecount(charLength) const wchar_t * content, charcount_t charLength, ScriptContext * scriptContext);
        static JavascriptString* NewCopySz(__in_z const wchar_t* content, ScriptContext* scriptContext);
        static JavascriptString* NewCopyBuffer(__in_ecount(charLength)  const wchar_t* content, charcount_t charLength, ScriptContext* scriptContext);

        static JavascriptString* NewWithArenaSz(__in_z const wchar_t * content, ScriptContext* scriptContext);
        static JavascriptString* NewWithArenaBuffer(__in_ecount(charLength) const wchar_t * content, charcount_t charLength, ScriptContext * scriptContext);

        static JavascriptString* NewCopySzFromArena(__in_z const wchar_t* content, ScriptContext* scriptContext, ArenaAllocator *arena);

        static __ecount(length+1) wchar_t* AllocateLeafAndCopySz(__in Recycler* recycler, __in_ecount(length) const wchar_t* content, charcount_t length);
        static __ecount(length+1) wchar_t* AllocateAndCopySz(__in ArenaAllocator* arena, __in_ecount(length) const wchar_t* content, charcount_t length);
        static void CopyHelper(__out_ecount(countNeeded) wchar_t *dst, __in_ecount(countNeeded) const wchar_t * str, charcount_t countNeeded);

    public:
        JavascriptString* ConcatDestructive(JavascriptString* pstRight);
    private:
        JavascriptString* ConcatDestructive_Compound(JavascriptString* pstRight);
        JavascriptString* ConcatDestructive_ConcatToCompound(JavascriptString* pstRight);
        JavascriptString* ConcatDestructive_OneEmpty(JavascriptString* pstRight);
        JavascriptString* ConcatDestructive_CompoundAppendChars(JavascriptString* pstRight);

    public:
        static JavascriptString* Concat(JavascriptString * pstLeft, JavascriptString * pstRight);
        static JavascriptString* Concat3(JavascriptString * pstLeft, JavascriptString * pstCenter, JavascriptString * pstRight);
    private:
        static JavascriptString* Concat_Compound(JavascriptString * pstLeft, JavascriptString * pstRight);
        static JavascriptString* Concat_ConcatToCompound(JavascriptString * pstLeft, JavascriptString * pstRight);
        static JavascriptString* Concat_OneEmpty(JavascriptString * pstLeft, JavascriptString * pstRight);
        static JavascriptString* Concat_BothOneChar(JavascriptString * pstLeft, JavascriptString * pstRight);

    public:
        static uint32 GetOffsetOfpszValue()
        {
            return offsetof(JavascriptString, m_pszValue);
        }

        static uint32 GetOffsetOfcharLength()
        {
            return offsetof(JavascriptString, m_charLength);
        }


        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo CharAt;
            static FunctionInfo CharCodeAt;
            static FunctionInfo CodePointAt;
            static FunctionInfo Concat;
            static FunctionInfo FromCharCode;
            static FunctionInfo FromCodePoint;
            static FunctionInfo IndexOf;
            static FunctionInfo LastIndexOf;
            static FunctionInfo LocaleCompare;
            static FunctionInfo Match;
            static FunctionInfo Normalize;
            static FunctionInfo Raw;
            static FunctionInfo Replace;
            static FunctionInfo Search;
            static FunctionInfo Slice;
            static FunctionInfo Split;
            static FunctionInfo Substring;
            static FunctionInfo Substr;
            static FunctionInfo ToLocaleLowerCase;
            static FunctionInfo ToLocaleUpperCase;
            static FunctionInfo ToLowerCase;
            static FunctionInfo ToString;
            static FunctionInfo ToUpperCase;
            static FunctionInfo Trim;
            static FunctionInfo TrimLeft;
            static FunctionInfo TrimRight;
            static FunctionInfo Repeat;
            static FunctionInfo StartsWith;
            static FunctionInfo EndsWith;
            static FunctionInfo Includes;
#ifdef TAGENTRY
#undef TAGENTRY
#endif
#define TAGENTRY(name, ...) static FunctionInfo name;
#include "JavascriptStringTagEntries.h"
#undef TAGENTRY
            static FunctionInfo ValueOf;
            static FunctionInfo SymbolIterator;
        };

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryCharAt(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryCharCodeAt(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryCodePointAt(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryConcat(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFromCharCode(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFromCodePoint(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIndexOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryLastIndexOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryLocaleCompare(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryMatch(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryNormalize(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryRaw(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReplace(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySearch(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySlice(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySplit(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySubstring(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySubstr(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToLocaleLowerCase(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToLocaleUpperCase(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToLowerCase(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToString(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryToUpperCase(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryTrim(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryTrimLeft(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryTrimRight(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryRepeat(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryStartsWith(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEndsWith(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryIncludes(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryAnchor(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryBig(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryBlink(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryBold(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFixed(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFontColor(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFontSize(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryItalics(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryLink(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySmall(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryStrike(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySub(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySup(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySymbolIterator(RecyclableObject* function, CallInfo callInfo, ...);

        static Var SubstringCore(JavascriptString* str, int start, int span, ScriptContext* scriptContext);
        static charcount_t GetBufferLength(const wchar_t *content);
        static charcount_t GetBufferLength(const wchar_t *content, int charLengthOrMinusOne);
        static bool IsASCII7BitChar(wchar_t ch) { return ch < 0x0080; }
        static char ToASCII7BitChar(wchar_t ch) { Assert(IsASCII7BitChar(ch)); return static_cast<char>(ch); }

    private:
        static int IndexOf(ArgumentReader& args, ScriptContext* scriptContext, const wchar_t* apiNameForErrorMsg, bool isRegExpAnAllowedArg);
        static void GetThisStringArgument(ArgumentReader& args, ScriptContext* scriptContext, const wchar_t* apiNameForErrorMsg, JavascriptString** ppThis);
        static void GetThisAndSearchStringArguments(ArgumentReader& args, ScriptContext* scriptContext, const wchar_t* apiNameForErrorMsg, JavascriptString** ppThis, JavascriptString** ppSearch, bool isRegExpAnAllowedArg);

        static BOOL GetThisValueVar(Var aValue, JavascriptString** pString, ScriptContext* scriptContext);
        static Var StringBracketHelper(Arguments args, ScriptContext *scriptContext, __in_ecount(cchTag) wchar_t const*pszTag, charcount_t cchTag,
                                        __in_ecount_opt(cchProp) wchar_t const*pszProp, charcount_t cchProp);

        template< size_t N >
        static Var StringBracketHelper(Arguments args, ScriptContext *scriptContext, const wchar_t (&tag)[N]);

        template< size_t N1, size_t N2 >
        static Var StringBracketHelper(Arguments args, ScriptContext *scriptContext, const wchar_t (&tag)[N1], const wchar_t (&prop)[N2]);

        static void SearchValueHelper(ScriptContext* scriptContext, Var aValue, JavascriptRegExp ** ppSearchRegEx, JavascriptString ** ppSearchString);
        static void ReplaceValueHelper(ScriptContext* scriptContext, Var aValue, JavascriptFunction ** ppReplaceFn, JavascriptString ** ppReplaceString);

        static Var ToLocaleCaseHelper(Var thisObj, bool toUpper, ScriptContext *scriptContext);

        static void InstantiateForceInlinedMembers();

        template <bool trimLeft, bool trimRight>
        static Var TrimLeftRightHelper(JavascriptString* arg, ScriptContext* scriptContext);
    };

    template<>
    struct PropertyRecordStringHashComparer<JavascriptString *>
    {
        __inline static bool Equals(JavascriptString * str1, JavascriptString * str2)
        {
            return (str1->GetLength() == str2->GetLength() &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1->GetString(), str2->GetString(), str1->GetLength()));
        }

        __inline static bool Equals(JavascriptString * str1, JsUtil::CharacterBuffer<WCHAR> const & str2)
        {
            return (str1->GetLength() == str2.GetLength() &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1->GetString(), str2.GetBuffer(), str1->GetLength()));
        }

        __inline static bool Equals(JavascriptString * str1, PropertyRecord const * str2)
        {
            return (str1->GetLength() == str2->GetLength() && !Js::IsInternalPropertyId(str2->GetPropertyId()) &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1->GetString(), str2->GetBuffer(), str1->GetLength()));
        }

        __inline static uint GetHashCode(JavascriptString * str)
        {
            return JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(str->GetString(), str->GetLength());
        }
    };

    __inline bool PropertyRecordStringHashComparer<PropertyRecord const *>::Equals(PropertyRecord const * str1, JavascriptString * str2)
    {
        return (str1->GetLength() == str2->GetLength() && !Js::IsInternalPropertyId(str1->GetPropertyId()) &&
            JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1->GetBuffer(), str2->GetString(), str1->GetLength()));
    }
}

template <>
struct DefaultComparer<Js::JavascriptString*>
{
    __inline static bool Equals(Js::JavascriptString * x, Js::JavascriptString * y)
    {
        return Js::JavascriptString::Equals(x, y);
    }

    __inline static uint GetHashCode(Js::JavascriptString * pStr)
    {
        return JsUtil::CharacterBuffer<wchar_t>::StaticGetHashCode(pStr->GetString(), pStr->GetLength());
    }
};
