//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptSet : public DynamicObject
    {
    public:
        typedef MapOrSetDataNode<Var> SetDataNode;
        typedef MapOrSetDataList<Var> SetDataList;
        typedef JsUtil::BaseDictionary<Var, SetDataNode*, Recycler, PowerOf2SizePolicy, SameValueZeroComparer> SetDataSet;

    private:
        SetDataList list;
        SetDataSet* set;

        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptSet, DynamicObject, list);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptSet);

    public:
        JavascriptSet(DynamicType* type);

        static JavascriptSet* New(ScriptContext* scriptContext);

        static bool Is(Var aValue);
        static JavascriptSet* FromVar(Var aValue);

        void Add(Var value);
        void Clear();
        bool Delete(Var value);
        bool Has(Var value);
        int Size();

        SetDataList::Iterator GetIterator();

        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        class EntryInfo
        {
        public:
            static FunctionInfo NewInstance;
            static FunctionInfo Add;
            static FunctionInfo Clear;
            static FunctionInfo Delete;
            static FunctionInfo ForEach;
            static FunctionInfo Has;
            static FunctionInfo SizeGetter;
            static FunctionInfo Entries;
            static FunctionInfo Values;
            static FunctionInfo GetterSymbolSpecies;
        };
        static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryAdd(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryClear(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryDelete(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryForEach(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryHas(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySizeGetter(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEntries(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryValues(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...);
    };
}
