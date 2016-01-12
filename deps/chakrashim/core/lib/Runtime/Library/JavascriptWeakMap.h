//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    typedef void* WeakMapId;

    class JavascriptWeakMap : public DynamicObject
    {
    private:
        // WeakMapKeyMap is the data that is kept alive by the key object itself so
        // that the lifetime of the WeakMap mapping from key to value is tied to the
        // lifetime of the key object. The WeakMap itself contains only a weak
        // reference to this data for the purpose of removing all references in the
        // Clear() and Finalize() methods, as well as enumeration in the debugger.
        //
        // Currently the WeakMapKeyMap object is stored in an internal property slot
        // on the key object. This is a problem though because making an object the
        // key of a WeakMap will then change the object's type and invalidate
        // caching and JIT assumptions.
        //
        // One alternative idea to using an internal property slot to hold the
        // WeakMapKeyMap on the key object is to use the arrayOrFlags field on
        // DynamicObject. E.g. subclass JavascriptArray with a version that is
        // both an array and has the extra data on it, allowing it to be placed in
        // the arrayOrFlags field. There are problems with this approach though:
        //
        //   1. arrayOrFlags is tied to sensitive performance optimizations and
        //      changing it will be met with difficulty in trying to maintain
        //      current benchmark performance
        //
        //   2. Not all objects are DynamicObject, so there are still keys that
        //      will not have the arrayOrFlags field (e.g. HostDispatch). Perhaps
        //      such objects should not be permitted to be keys on WeakMap?
        //
        // Regardless of this idea, the ideal would be to have InternalPropertyIds
        // not affect the type of an object. That is, ideally we would have a way
        // to add and remove InternalPropertyIds from an object without affecting
        // its type and therefore without invalidating cache and JIT assumptions.
        //
        typedef JsUtil::BaseDictionary<WeakMapId, Var, Recycler, PowerOf2SizePolicy, RecyclerPointerComparer> WeakMapKeyMap;
        typedef JsUtil::WeaklyReferencedKeyDictionary<DynamicObject, bool, RecyclerPointerComparer<const DynamicObject*>> KeySet;

        KeySet keySet;

        WeakMapKeyMap* GetWeakMapKeyMapFromKey(DynamicObject* key) const;
        WeakMapKeyMap* AddWeakMapKeyMapToKey(DynamicObject* key);

        WeakMapId GetWeakMapId() const { return (void*)(((uintptr)this) | 1); }
        static JavascriptWeakMap* GetWeakMapFromId(WeakMapId id) { return reinterpret_cast<JavascriptWeakMap*>((uintptr)id & (~1)); }

        bool KeyMapGet(WeakMapKeyMap* map, Var* value) const;

        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptWeakMap, DynamicObject, keySet);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptWeakMap);

    public:
        JavascriptWeakMap(DynamicType* type);

        static bool Is(Var aValue);
        static JavascriptWeakMap* FromVar(Var aValue);

        void Clear();
        bool Delete(DynamicObject* key);
        bool Get(DynamicObject* key, Var* value) const;
        bool Has(DynamicObject* key) const;
        void Set(DynamicObject* key, Var value);

        virtual void Finalize(bool isShutdown) override { Clear(); }
        virtual void Dispose(bool isShutdown) override { }

        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo Delete;
            static FunctionInfo Get;
            static FunctionInfo Has;
            static FunctionInfo Set;
        };

        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryDelete(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGet(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryHas(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySet(RecyclableObject* function, CallInfo callInfo, ...);

    public:
        // For diagnostics and heap enum provide size and allow enumeration of key value pairs
        int Size() { keySet.Clean(); return keySet.Count(); }
        template <typename Fn>
        void Map(Fn fn)
        {
            return keySet.Map([&](DynamicObject* key, bool, const RecyclerWeakReference<DynamicObject>*)
            {
                Var value = nullptr;
                WeakMapKeyMap* keyMap = GetWeakMapKeyMapFromKey(key);

                // It may be the case that a CustomExternalObject (CEO) was reset, removing its WeakMapKeyMap.
                // In this case it can still be in the keySet. The keyMap may be null because of the reset,
                // but it could be reinstated if the CEO was added to another WeakMap. Thus it could also be the case that
                // this WeakMap's ID is not in the WeakMapKeyMap returned from the key object. Ignore the
                // CEO key object in these two cases.
                if (keyMap != nullptr && keyMap->ContainsKey(GetWeakMapId()))
                {
                    KeyMapGet(keyMap, &value);
                    fn(key, value);
                }
            });
        }
    };
}
