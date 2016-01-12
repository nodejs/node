//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
#ifdef PROFILE_RECYCLER_ALLOC
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
class RecyclerObjectDumper
{
public:
    typedef bool (*DumpFunction)(type_info const * typeinfo, bool isArray, void * objectAddress);
    static void RegisterDumper(type_info const * typeinfo, DumpFunction dumperFunction);
    static void DumpObject(type_info const * typeinfo, bool isArray, void * objectAddress);
private:
    RecyclerObjectDumper() {}
    ~RecyclerObjectDumper();
    static RecyclerObjectDumper Instance;
    static BOOL EnsureDumpFunctionMap();

    typedef JsUtil::BaseDictionary<type_info const *, RecyclerObjectDumper::DumpFunction, NoCheckHeapAllocator> DumpFunctionMap;
    static DumpFunctionMap * dumpFunctionMap;
};

template <typename T, RecyclerObjectDumper::DumpFunction dumpFunction>
class AutoRegisterRecyclerObjectDumper
{
public:
    static AutoRegisterRecyclerObjectDumper Instance;
private:
    AutoRegisterRecyclerObjectDumper()
    {
        RecyclerObjectDumper::RegisterDumper(&typeid(T), dumpFunction);
    }
};
template <typename T, RecyclerObjectDumper::DumpFunction dumpFunction>
AutoRegisterRecyclerObjectDumper<T, dumpFunction> AutoRegisterRecyclerObjectDumper<T, dumpFunction>::Instance;

#define AUTO_REGISTER_RECYCLER_OBJECT_DUMPER(T, func) template AutoRegisterRecyclerObjectDumper<T, func>;
#else
#define AUTO_REGISTER_RECYCLER_OBJECT_DUMPER(T, func)
#endif

void DumpRecyclerObjectGraph();
#else
#define AUTO_REGISTER_RECYCLER_OBJECT_DUMPER(T, func)
#endif
}
