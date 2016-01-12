//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define MAX_SIZE_PATH_LENGTH (128)

namespace Js
{
    class TinyDictionary
    {
        static const int PowerOf2_BUCKETS = 8;
        static const byte NIL = 0xff;
        static const int NEXTPTRCOUNT = MAX_SIZE_PATH_LENGTH;

        byte buckets[PowerOf2_BUCKETS];
        byte next[NEXTPTRCOUNT];

public:
        TinyDictionary()
        {
            DWORD* init = (DWORD*)buckets;
            init[0] = init[1] = 0xffffffff;
        }

        void Add(PropertyId key, byte value)
        {
            Assert(value < NEXTPTRCOUNT);
            __analysis_assume(value < NEXTPTRCOUNT);

            uint32 bucketIndex = key&(PowerOf2_BUCKETS-1);

            byte i = buckets[bucketIndex];
            buckets[bucketIndex] = value;
            next[value] = i;
        }

        // Template shared with diagnostics
        template <class Data>
        __inline bool TryGetValue(PropertyId key, PropertyIndex* index, const Data& data)
        {
            uint32 bucketIndex = key&(PowerOf2_BUCKETS-1);

            for (byte i = buckets[bucketIndex] ; i != NIL ; i = next[i])
            {
                Assert(i < NEXTPTRCOUNT);
                __analysis_assume(i < NEXTPTRCOUNT);

                if (data[i]->GetPropertyId()== key)
                {
                    *index = i;
                    return true;
                }
                Assert(i != next[i]);
            }
            return false;
        }
    };

    class TypePath
    {
        friend class PathTypeHandlerBase;
        friend class DynamicObject;
        friend class SimplePathTypeHandler;
        friend class PathTypeHandler;

    public:
        static const uint MaxPathTypeHandlerLength = MAX_SIZE_PATH_LENGTH;
        static const uint InitialTypePathSize = 16;

    private:
        TinyDictionary map;
        uint16 pathLength;      // Entries in use
        uint16 pathSize;        // Allocated entries

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        // We sometimes set up PathTypeHandlers and associate TypePaths before we create any instances
        // that populate the corresponding slots, e.g. for object literals or constructors with only
        // this statements.  This field keeps track of the longest instance associated with the given
        // TypePath.
        int maxInitializedLength;
        RecyclerWeakReference<DynamicObject>* singletonInstance;
        BVStatic<MAX_SIZE_PATH_LENGTH> fixedFields;
        BVStatic<MAX_SIZE_PATH_LENGTH> usedFixedFields;
#endif

        // PropertyRecord assignments are allocated off the end of the structure
        const PropertyRecord * assignments[0];

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        TypePath()
            : pathLength(0), maxInitializedLength(0), singletonInstance(nullptr)
        {
        }
#else
        TypePath()
            : pathLength(0)
        {
        }
#endif

    public:
        static TypePath* New(Recycler* recycler, uint size = InitialTypePathSize);

        TypePath * Branch(Recycler * alloc, int pathLength, bool couldSeeProto);

        TypePath * Grow(Recycler * alloc);

        const PropertyRecord* GetPropertyIdUnchecked(int index)
        {
            Assert(((uint)index) < ((uint)pathLength));
            return assignments[index];
        }

        const PropertyRecord* GetPropertyId(int index)
        {
            if (((uint)index) < ((uint)pathLength))
                return GetPropertyIdUnchecked(index);
            else
                return nullptr;
        }

        const PropertyRecord ** GetPropertyAssignments()
        {
            return assignments;
        }

        int Add(const PropertyRecord * propertyRecord)
        {
#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
            Assert(this->pathLength == this->maxInitializedLength);
            this->maxInitializedLength++;
#endif
            return AddInternal(propertyRecord);
        }

        uint16 GetPathLength() { return this->pathLength; }
        uint16 GetPathSize() const { return this->pathSize; }

        PropertyIndex Lookup(PropertyId propId,int typePathLength);
        PropertyIndex LookupInline(PropertyId propId,int typePathLength);

    private:
        int AddInternal(const PropertyRecord* propId);

#ifdef SUPPORT_FIXED_FIELDS_ON_PATH_TYPES
        int GetMaxInitializedLength() { return this->maxInitializedLength; }
        void SetMaxInitializedLength(int newMaxInitializedLength)
        {
            Assert(this->maxInitializedLength <= newMaxInitializedLength);
            this->maxInitializedLength = newMaxInitializedLength;
        }

        Var GetSingletonFixedFieldAt(PropertyIndex index, int typePathLength, ScriptContext * requestContext);

        bool HasSingletonInstance() const
        {
            return this->singletonInstance != nullptr;
        }

        RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const
        {
            return this->singletonInstance;
        }

        void SetSingletonInstance(RecyclerWeakReference<DynamicObject>* instance, int typePathLength)
        {
            Assert(this->singletonInstance == nullptr && instance != nullptr);
            Assert(typePathLength >= this->maxInitializedLength);
            this->singletonInstance = instance;
        }

        void ClearSingletonInstance()
        {
            this->singletonInstance = nullptr;
        }

        void ClearSingletonInstanceIfSame(DynamicObject* instance)
        {
            if (this->singletonInstance != nullptr && this->singletonInstance->Get() == instance)
            {
                ClearSingletonInstance();
            }
        }

        void ClearSingletonInstanceIfDifferent(DynamicObject* instance)
        {
            if (this->singletonInstance != nullptr && this->singletonInstance->Get() != instance)
            {
                ClearSingletonInstance();
            }
        }

        bool GetIsFixedFieldAt(PropertyIndex index, int typePathLength)
        {
            Assert(index < this->pathLength);
            Assert(index < typePathLength);
            Assert(typePathLength <= this->pathLength);

            return this->fixedFields.Test(index) != 0;
        }

        bool GetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength)
        {
            Assert(index < this->pathLength);
            Assert(index < typePathLength);
            Assert(typePathLength <= this->pathLength);

            return this->usedFixedFields.Test(index) != 0;
        }

        void SetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength)
        {
            Assert(index < this->maxInitializedLength);
            Assert(CanHaveFixedFields(typePathLength));
            this->usedFixedFields.Set(index);
        }

        void ClearIsFixedFieldAt(PropertyIndex index, int typePathLength)
        {
            Assert(index < this->maxInitializedLength);
            Assert(index < typePathLength);
            Assert(typePathLength <= this->pathLength);

            this->fixedFields.Clear(index);
            this->usedFixedFields.Clear(index);
        }

        bool CanHaveFixedFields(int typePathLength)
        {
            // We only support fixed fields on singleton instances.
            // If the instance in question is a singleton, it must be the tip of the type path.
            return this->singletonInstance != nullptr && typePathLength >= this->maxInitializedLength;
        }

        void AddBlankFieldAt(PropertyIndex index, int typePathLength);

        void AddSingletonInstanceFieldAt(DynamicObject* instance, PropertyIndex index, bool isFixed, int typePathLength);

        void AddSingletonInstanceFieldAt(PropertyIndex index, int typePathLength);

#if DBG
        bool HasSingletonInstanceOnlyIfNeeded();
#endif

#else
        int GetMaxInitializedLength() { Assert(false); return this->pathLength; }

        Var GetSingletonFixedFieldAt(PropertyIndex index, int typePathLength, ScriptContext * requestContext);

        bool HasSingletonInstance() const { Assert(false); return false; }
        RecyclerWeakReference<DynamicObject>* GetSingletonInstance() const { Assert(false); return nullptr; }
        void SetSingletonInstance(RecyclerWeakReference<DynamicObject>* instance, int typePathLength) { Assert(false); }
        void ClearSingletonInstance() { Assert(false); }
        void ClearSingletonInstanceIfSame(RecyclerWeakReference<DynamicObject>* instance) { Assert(false); }
        void ClearSingletonInstanceIfDifferent(RecyclerWeakReference<DynamicObject>* instance) { Assert(false); }

        bool GetIsFixedFieldAt(PropertyIndex index, int typePathLength) { Assert(false); return false; }
        bool GetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength) { Assert(false); return false; }
        void SetIsUsedFixedFieldAt(PropertyIndex index, int typePathLength) { Assert(false); }
        void ClearIsFixedFieldAt(PropertyIndex index, int typePathLength) { Assert(false); }
        bool CanHaveFixedFields(int typePathLength) { Assert(false); return false; }
        void AddBlankFieldAt(PropertyIndex index, int typePathLength) { Assert(false); }
        void AddSingletonInstanceFieldAt(DynamicObject* instance, PropertyIndex index, bool isFixed, int typePathLength) { Assert(false); }
        void AddSingletonInstanceFieldAt(PropertyIndex index, int typePathLength) { Assert(false); }
#if DBG
        bool HasSingletonInstanceOnlyIfNeeded();
#endif
#endif
    };
}

