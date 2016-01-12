//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(Type);

    InternalString Type::ObjectTypeNameString    = InternalString(L"object", 6);
    InternalString Type::UndefinedTypeNameString = InternalString(L"undefined", 9);
    InternalString Type::BooleanTypeNameString   = InternalString(L"boolean", 7);
    InternalString Type::StringTypeNameString    = InternalString(L"string", 6);
    InternalString Type::NumberTypeNameString    = InternalString(L"number", 6);
    InternalString Type::FunctionTypeNameString  = InternalString(L"function", 8);

    Type::Type(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint) :
        javascriptLibrary(scriptContext->GetLibrary()),
        typeId(typeId),
        prototype(prototype),
        propertyCache(nullptr),
        flags(TypeFlagMask_None)
    {
#ifdef PROFILE_TYPES
        if (typeId < sizeof(scriptContext->typeCount)/sizeof(int))
        {
            scriptContext->typeCount[typeId]++;
        }
#endif
        this->entryPoint = entryPoint != nullptr ? entryPoint : RecyclableObject::DefaultEntryPoint;
        if (prototype)
        {
            Assert(! CrossSite::NeedMarshalVar(prototype,scriptContext));
            prototype->SetIsPrototype();
        }
    }

    Type::Type(Type * type) :
        typeId(type->typeId),
        javascriptLibrary(type->javascriptLibrary),
        prototype(type->prototype),
        entryPoint(type->entryPoint),
        flags(type->flags),
        propertyCache(nullptr)
    {
#ifdef PROFILE_TYPES
        if (typeId < sizeof(javascriptLibrary->GetScriptContext()->typeCount)/sizeof(int))
        {
            javascriptLibrary->GetScriptContext()->typeCount[typeId]++;
        }
#endif
        Assert(! (prototype && CrossSite::NeedMarshalVar(prototype, javascriptLibrary->GetScriptContext())));

        // If the type property cache is copied over to this new type, then if a property ID caused the type to be changed for
        // the purpose of invalidating caches due to the property being deleted or its attributes being changed, then the cache
        // for that property ID must be cleared on this new type after the type property cache is copied. Also, types are not
        // changed consistently to use this copy constructor, so those would need to be fixed as well.

        if(type->AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties())
        {
            SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(true);
        }
        if(type->IsFalsy())
        {
            SetIsFalsy(true);
        }
    }

    ScriptContext *
    Type::GetScriptContext() const
    {
        return GetLibrary()->GetScriptContext();
    }

    Recycler *
    Type::GetRecycler() const
    {
        return GetLibrary()->GetRecycler();
    }

    TypePropertyCache *Type::GetPropertyCache()
    {
        return propertyCache;
    }

    TypePropertyCache *Type::CreatePropertyCache()
    {
        Assert(!propertyCache);

        propertyCache = RecyclerNew(GetRecycler(), TypePropertyCache);
        return propertyCache;
    }

    void Type::SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(const bool truth)
    {
        if (truth)
        {
            if (GetScriptContext()->IsClosed())
            {
                // The cache is disabled after the script context is closed, to avoid issues between being closed and being deleted,
                // where the cache of these types in JavascriptLibrary may be reclaimed at any point
                return;
            }

            flags |= TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties;
            javascriptLibrary->TypeAndPrototypesAreEnsuredToHaveOnlyWritableDataProperties(this);
        }
        else
        {
            flags &= ~TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties;
        }
    }

    BOOL Type::AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties() const
    {
        return flags & TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties;
    }

    BOOL Type::IsFalsy() const
    {
        return flags & TypeFlagMask_IsFalsy;
    }

    void Type::SetIsFalsy(const bool truth)
    {
        if (truth)
        {
            Assert(this->GetScriptContext()->GetThreadContext()->CanBeFalsy(this->GetTypeId()));
            flags |= TypeFlagMask_IsFalsy;
        }
        else
        {
            flags &= ~TypeFlagMask_IsFalsy;
        }
    }

    void Type::SetHasSpecialPrototype(const bool truth)
    {
        if (truth)
        {
            flags |= TypeFlagMask_HasSpecialPrototype;
        }
        else
        {
            flags &= ~TypeFlagMask_HasSpecialPrototype;
        }
    }

    uint32 Type::GetOffsetOfTypeId()
    {
        return offsetof(Type, typeId);
    }

    uint32 Type::GetOffsetOfFlags()
    {
        return offsetof(Type, flags);
    }

    uint32 Type::GetOffsetOfEntryPoint()
    {
        return offsetof(Type, entryPoint);
    }

    uint32 Type::GetOffsetOfPrototype()
    {
        return offsetof(Type, prototype);
    }

#if defined(PROFILE_RECYCLER_ALLOC) && defined(RECYCLER_DUMP_OBJECT_GRAPH)
    bool Type::DumpObjectFunction(type_info const * typeinfo, bool isArray, void * objectAddress)
    {
        if (isArray)
        {
            // Don't deal with array
            return false;
        }

        Output::Print(L"%S{%x} %p", typeinfo->name(), ((Type *)objectAddress)->GetTypeId(), objectAddress);
        return true;
    }
#endif
}
