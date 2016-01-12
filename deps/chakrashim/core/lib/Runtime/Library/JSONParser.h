//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#include <wchar.h>
#include "JSONScanner.h"
namespace JSON
{
    class JSONDeferredParserRootNode;

    struct JsonTypeCache
    {
        const Js::PropertyRecord* propertyRecord;
        Js::DynamicType* typeWithoutProperty;
        Js::DynamicType* typeWithProperty;
        JsonTypeCache* next;
        Js::PropertyIndex propertyIndex;

        JsonTypeCache(const Js::PropertyRecord* propertyRecord, Js::DynamicType* typeWithoutProperty, Js::DynamicType* typeWithProperty, Js::PropertyIndex propertyIndex) :
            propertyRecord(propertyRecord),
            typeWithoutProperty(typeWithoutProperty),
            typeWithProperty(typeWithProperty),
            propertyIndex(propertyIndex),
            next(nullptr) {}

        static JsonTypeCache* JsonTypeCache::New(ArenaAllocator* allocator,
            const Js::PropertyRecord* propertyRecord,
            Js::DynamicType* typeWithoutProperty,
            Js::DynamicType* typeWithProperty,
            Js::PropertyIndex propertyIndex)
        {
            return Anew(allocator, JsonTypeCache, propertyRecord, typeWithoutProperty, typeWithProperty, propertyIndex);
        }

        void Update(const Js::PropertyRecord* propertyRecord,
            Js::DynamicType* typeWithoutProperty,
            Js::DynamicType* typeWithProperty,
            Js::PropertyIndex propertyIndex)
        {
            this->propertyRecord = propertyRecord;
            this->typeWithoutProperty = typeWithoutProperty;
            this->typeWithProperty = typeWithProperty;
            this->propertyIndex = propertyIndex;
        }
    };


    class JSONParser
    {
    public:
        JSONParser(Js::ScriptContext* sc, Js::RecyclableObject* rv) : scriptContext(sc),
            reviver(rv),  arenaAllocatorObject(nullptr), arenaAllocator(nullptr), typeCacheList(nullptr)
        {
        };

        Js::Var Parse(LPCWSTR str, int length);
        Js::Var Parse(Js::JavascriptString* input);
        Js::Var Walk(Js::JavascriptString* name, Js::PropertyId id, Js::Var holder, uint32 index = Js::JavascriptArray::InvalidIndex);
        void Finalizer();

    private:
        tokens Scan()
        {
            return m_scanner.Scan();
        }

        Js::Var ParseObject();

        void CheckCurrentToken(int tk, int wErr)
        {
            if (m_token.tk != tk)
                Js::JavascriptError::ThrowSyntaxError(scriptContext, wErr);
            Scan();
        }

        bool IsCaching()
        {
            return arenaAllocator != nullptr;
        }

        Token m_token;
        JSONScanner m_scanner;
        Js::ScriptContext* scriptContext;
        Js::RecyclableObject* reviver;
        Js::TempGuestArenaAllocatorObject* arenaAllocatorObject;
        ArenaAllocator* arenaAllocator;
        typedef JsUtil::BaseDictionary<const Js::PropertyRecord *, JsonTypeCache*, ArenaAllocator, PowerOf2SizePolicy, Js::PropertyRecordStringHashComparer>  JsonTypeCacheList;
        JsonTypeCacheList* typeCacheList;
        static const int MIN_CACHE_LENGTH = 50; // Use Json type cache only if the JSON string is larger than this constant.
    };
} // namespace JSON
