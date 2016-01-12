//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
#if ENABLE_NATIVE_CODEGEN
    template <typename T>
    class BranchDictionaryWrapper
    {
    public:
        typedef JsUtil::BaseDictionary<T, void*, NativeCodeData::Allocator> BranchDictionary;

        BranchDictionaryWrapper(NativeCodeData::Allocator * allocator, uint dictionarySize) :
            defaultTarget(nullptr), dictionary(allocator)
        {
        }

        BranchDictionary dictionary;
        void* defaultTarget;

        static BranchDictionaryWrapper* New(NativeCodeData::Allocator * allocator, uint dictionarySize)
        {
            return NativeCodeDataNew(allocator, BranchDictionaryWrapper, allocator, dictionarySize);
        }

    };

    class JavascriptNativeOperators
    {
    public:
        static void * Op_SwitchStringLookUp(JavascriptString* str, Js::BranchDictionaryWrapper<Js::JavascriptString*>* stringDictionary, uintptr funcStart, uintptr funcEnd);
    };
#endif
};
