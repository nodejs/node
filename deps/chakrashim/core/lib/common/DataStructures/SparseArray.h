//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template<class T>
class SAChunk
{
public:
    SAChunk<T> *        next;
    uint32              startIndex;
    T *                 data[];
};


template<class T>
class SparseArray
{
private:
    ArenaAllocator *    alloc;
    uint32              chunkSize;
    SAChunk<T> *        firstChunk;

public:
    static SparseArray<T> * New(ArenaAllocator *allocator, uint32 chunkSize)
    {
        SparseArray<T> * array;

        if (!Math::IsPow2(chunkSize))
        {
            chunkSize = Math::NextPowerOf2(chunkSize);
        }

        // Throw early if this overflows, since chunkSize never changes, subsequent operations will be safe
        UInt32Math::MulAdd<sizeof(T*), sizeof(SAChunk<T>)>(chunkSize);
        array = Anew(allocator, SparseArray<T>);
        array->alloc = allocator;
        array->chunkSize = chunkSize;
        array->firstChunk = NULL;

        return array;
    }

    void Set(uint32 index, T *element)
    {
        SAChunk<T> * chunk, **pPrev = &(this->firstChunk);
        uint32 indexInChunk = (index % this->chunkSize);

        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {
            if (index < chunk->startIndex)
            {
                // Need a new chunk...
                chunk = NULL;
                break;
            }
            if (index < chunk->startIndex + this->chunkSize)
            {
                break;
            }
            pPrev = &(chunk->next);
        }

        if (chunk == NULL)
        {
            chunk = (SAChunk<T> *)this->alloc->AllocZero(sizeof(SAChunk<T>) + (chunkSize * sizeof(T *)));
            chunk->startIndex = index - indexInChunk;
            // Since startIndex and chunkSize don't change, check now if this overflows.
            // Cache the result or save memory ?
            UInt32Math::Add(chunk->startIndex, chunkSize);
            chunk->next = *pPrev;
            *pPrev = chunk;
        }
        chunk->data[indexInChunk] = element;
    }

    T * Get(uint32 index)
    {
        SAChunk<T> * chunk;
        uint32 indexInChunk = (index % this->chunkSize);

        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {
            if (index < chunk->startIndex)
            {
                return NULL;
            }
            if (index < chunk->startIndex + this->chunkSize)
            {
                return chunk->data[indexInChunk];
            }
        }

        return NULL;
    }

    SparseArray<T> * Copy()
    {
        SparseArray<T> * newSA = SparseArray<T>::New(this->alloc, this->chunkSize);
        SAChunk<T> * chunk, *pred = NULL;

        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {
            SAChunk<T> *newChunk = (SAChunk<T> *)this->alloc->Alloc(sizeof(SAChunk<T>) + (sizeof(T *) * this->chunkSize));

            newChunk->startIndex = chunk->startIndex;
            js_memcpy_s(newChunk->data, sizeof(T *) * this->chunkSize, chunk->data, sizeof(T *) * this->chunkSize);
            if (pred)
            {
                pred->next = newChunk;
            }
            else
            {
                newSA->firstChunk = newChunk;
            }
            pred = newChunk;
        }

        if (pred)
        {
            pred->next = NULL;
        }
        else
        {
            newSA->firstChunk = NULL;
        }
        return newSA;
    }

    void And(SparseArray<T> *this2)
    {
        SAChunk<T> * chunk, *pred = NULL;
        SAChunk<T> * chunk2;

        AssertMsg(this->chunkSize == this2->chunkSize, "Anding incompatible arrays");
        chunk2 = this2->firstChunk;
        for (chunk = this->firstChunk; chunk; chunk = chunk->next)
        {
            while (chunk2 && chunk->startIndex > chunk2->startIndex)
            {
                chunk2 = chunk2->next;
            }

            if (chunk2 == NULL || chunk->startIndex < chunk2->startIndex)
            {
                if (pred)
                {
                    pred->next = chunk->next;
                }
                else
                {
                    this->firstChunk = chunk->next;
                }
                continue;
            }
            AssertMsg(chunk->startIndex == chunk2->startIndex, "Huh??");

            for (int i = 0; i < this->chunkSize; i++)
            {
                if (chunk->data[i])
                {
                    if (chunk2->data[i])
                    {
                        if (*(chunk->data[i]) == *(chunk2->data[i]))
                        {
                            continue;
                        }
                    }
                    chunk->data[i] = NULL;
                }
            }
            chunk2 = chunk2->next;
            pred = chunk;
       }
    }

    void Clear()
    {
        this->firstChunk = NULL;
    }

#if DBG_DUMP

    void Dump()
    {
        for (SAChunk<T> *chunk = this->firstChunk; chunk; chunk = chunk->next)
        {
            for (int index = chunk->startIndex; index < this->chunkSize; index++)
            {
                if (chunk->data[index])
                {
                    Output::Print(L"Index %4d  =>  ", index);
                    chunk->data[index]->Dump();
                    Output::Print(L"\n");
                }
            }
        }
    }

#endif
};
