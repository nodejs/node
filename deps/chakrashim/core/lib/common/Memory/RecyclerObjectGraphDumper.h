//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef RECYCLER_DUMP_OBJECT_GRAPH
class RecyclerObjectGraphDumper
{
public:
    struct Param
    {
        bool (*dumpReferenceFunc)(wchar_t const *, void *objectAddress, void *referenceAddress);
        bool dumpRootOnly;
        bool skipStack;
#ifdef RECYCLER_STATS
        RecyclerCollectionStats stats;
#endif
    };

    RecyclerObjectGraphDumper(Recycler * recycler, Param * param);
    ~RecyclerObjectGraphDumper();

    void BeginDumpObject(void * objectAddres);
    void BeginDumpObject(wchar_t const * name);
    void BeginDumpObject(wchar_t const * name, void* objectAddress);
    void EndDumpObject();
    void DumpObjectReference(void * objectAddress, bool remark);

    Recycler * recycler;
    Param * param;

    wchar_t const * dumpObjectName;
    wchar_t tempObjectName[256];
    void * dumpObject;

#ifdef PROFILE_RECYCLER_ALLOC
    type_info const * dumpObjectTypeInfo;
    bool dumpObjectIsArray;
#endif

    bool isOutOfMemory;
};
#endif

#ifdef RECYCLER_DUMP_OBJECT_GRAPH
#define BEGIN_DUMP_OBJECT(recycler, address) { if (recycler->objectGraphDumper != nullptr)  { recycler->objectGraphDumper->BeginDumpObject(address); }
#define BEGIN_DUMP_OBJECT_ADDRESS(name, address) { if (this->objectGraphDumper != nullptr) { this->objectGraphDumper->BeginDumpObject(name, address); }
#define DUMP_OBJECT_REFERENCE(recycler, address) if (recycler->objectGraphDumper != nullptr) { recycler->objectGraphDumper->DumpObjectReference(address, false); }
#define DUMP_OBJECT_REFERENCE_REMARK(recycler, address) if (recycler->objectGraphDumper != nullptr && recycler->IsValidObject(address)) { recycler->objectGraphDumper->DumpObjectReference(address, true); }
#define END_DUMP_OBJECT(recycler)  if (recycler->objectGraphDumper != nullptr)  { recycler->objectGraphDumper->EndDumpObject(); } }
#define DUMP_IMPLICIT_ROOT(recycler, address) BEGIN_DUMP_OBJECT(recycler, L"Implicit Root"); DUMP_OBJECT_REFERENCE(recycler, address); END_DUMP_OBJECT(recycler);
#else
#define BEGIN_DUMP_OBJECT(recycler, address)
#define BEGIN_DUMP_OBJECT_ADDRESS(name, address)
#define DUMP_OBJECT_REFERENCE(recycler, address)
#define DUMP_OBJECT_REFERENCE_REMARK(recycler, address)
#define END_DUMP_OBJECT(recycler)
#define DUMP_IMPLICIT_ROOT(recycler, address)
#endif
