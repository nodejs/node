//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptMap : public DynamicObject
    {
    public:
        typedef JsUtil::KeyValuePair<Var, Var> MapDataKeyValuePair;
        typedef MapOrSetDataNode<MapDataKeyValuePair> MapDataNode;
        typedef MapOrSetDataList<MapDataKeyValuePair> MapDataList;
        typedef JsUtil::BaseDictionary<Var, MapDataNode*, Recycler, PowerOf2SizePolicy, SameValueZeroComparer> MapDataMap;

    private:
        MapDataList list;
        MapDataMap* map;

        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptMap, DynamicObject, list);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptMap);

    public:
        JavascriptMap(DynamicType* type);

        static JavascriptMap* New(ScriptContext* scriptContext);

        static bool Is(Var aValue);
        static JavascriptMap* FromVar(Var aValue);

        void Clear();
        bool Delete(Var key);
        bool Get(Var key, Var* value);
        bool Has(Var key);
        void Set(Var key, Var value);
        int Size();

        MapDataList::Iterator GetIterator();

        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo Clear;
            static FunctionInfo Delete;
            static FunctionInfo ForEach;
            static FunctionInfo Get;
            static FunctionInfo Has;
            static FunctionInfo Set;
            static FunctionInfo SizeGetter;
            static FunctionInfo Entries;
            static FunctionInfo Keys;
            static FunctionInfo Values;
            static FunctionInfo GetterSymbolSpecies;
        };
        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryClear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryDelete(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryForEach(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGet(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryHas(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySet(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySizeGetter(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEntries(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryKeys(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryValues(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...);
    };
}
