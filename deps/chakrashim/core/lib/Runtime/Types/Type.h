//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum TypeFlagMask : uint8
{
    TypeFlagMask_None                                                              = 0x00,
    TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties       = 0x01,
    TypeFlagMask_IsFalsy                                                           = 0x02,
    TypeFlagMask_HasSpecialPrototype                                               = 0x04,
    TypeFlagMask_External                                                          = 0x08,
    TypeFlagMask_SkipsPrototype                                                    = 0x10,
    TypeFlagMask_CanHaveInterceptors                                               = 0x20,
};
ENUM_CLASS_HELPERS(TypeFlagMask, uint8);

namespace Js
{
    class TypePropertyCache;
    class Type
    {
        friend class DynamicObject;
        friend class GlobalObject;
        friend class ScriptEngineBase;

    protected:
        TypeId typeId;
        TypeFlagMask flags;

        JavascriptLibrary* javascriptLibrary;

        RecyclableObject* prototype;
        JavascriptMethod entryPoint;
    private:
        TypePropertyCache *propertyCache;
    protected:
        Type(Type * type);
        Type(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint);

    public:
        static DWORD GetJavascriptLibraryOffset() { return offsetof(Type, javascriptLibrary); }
        __inline TypeId GetTypeId() const { return typeId; }
        void SetTypeId(TypeId typeId) { this->typeId = typeId; }
        RecyclableObject* GetPrototype() const { return prototype; }
        JavascriptMethod GetEntryPoint() const { return entryPoint; }
        JavascriptLibrary* GetLibrary() const { return javascriptLibrary; }
        ScriptContext * GetScriptContext() const;
        Recycler * GetRecycler() const;
        TypePropertyCache *GetPropertyCache();
        TypePropertyCache *CreatePropertyCache();
        BOOL HasSpecialPrototype() const { return (flags & TypeFlagMask_HasSpecialPrototype) == TypeFlagMask_HasSpecialPrototype; }

        // This function has a different meaning from RecyclableObject::HasOnlyWritableDataProperties. If this function returns
        // true, then it's implied that RecyclableObject::HasOnlyWritableDataProperties would return true for an object of this
        // type and all of its prototypes. However, if this function returns false, it does not imply the converse.
        BOOL AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties() const;
        void SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(const bool truth);

        __inline BOOL IsExternal() const { return (this->flags & TypeFlagMask_External) != 0; }
        __inline BOOL SkipsPrototype() const { return (this->flags & TypeFlagMask_SkipsPrototype) != 0 ; }
        __inline BOOL CanHaveInterceptors() const { return (this->flags & TypeFlagMask_CanHaveInterceptors) != 0; }
        __inline BOOL IsFalsy() const;
        void SetIsFalsy(const bool truth);
        void SetHasSpecialPrototype(const bool hasSpecialPrototype);

        // This is for static lib verification use only.
        static DWORD GetTypeIdFieldOffset() { return offsetof(Type, typeId); }
        static size_t OffsetOfWritablePropertiesFlag()
        {
            return offsetof(Type, flags);
        }

        static uint32 GetOffsetOfTypeId();
        static uint32 GetOffsetOfFlags();
        static uint32 GetOffsetOfEntryPoint();
        static uint32 GetOffsetOfPrototype();

        static InternalString UndefinedTypeNameString;
        static InternalString ObjectTypeNameString;
        static InternalString BooleanTypeNameString;
        static InternalString NumberTypeNameString;
        static InternalString StringTypeNameString;
        static InternalString FunctionTypeNameString;

#if defined(PROFILE_RECYCLER_ALLOC) && defined(RECYCLER_DUMP_OBJECT_GRAPH)
        static bool DumpObjectFunction(type_info const * typeinfo, bool isArray, void * objectAddress);
#endif
    };
};
