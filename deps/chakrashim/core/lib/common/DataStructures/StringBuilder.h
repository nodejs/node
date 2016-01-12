//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template <typename TAllocator>
    class StringBuilder
    {
    private:
        struct Data
        {
        public:
            union {
                struct st_Single
                {
                    wchar_t buffer[];
                } single;

                struct st_Chained
                {
                    charcount_t length;
                    Data *next;
                    wchar_t buffer[];
                } chained;
            }u;
        };

    private:
        static const charcount_t MaxLength = INT_MAX - 1;

        const static charcount_t MaxRealloc = 64;
        TAllocator* alloc;
        // First chunk is just a buffer, and which can be detached without copying.
        Data *firstChunk;
        // Second chunk is a chained list of chunks.  UnChain() needs to be called to copy the first chunk
        // and the list of chained chunks to a single buffer on calls to GetBuffer().
        Data *secondChunk;
        Data *lastChunk;
        wchar_t * appendPtr;
        charcount_t length; // Total capacity (allocated number of elements - 1), in all chunks. Note that we keep one allocated element which is not accounted in length for terminating '\0'.
        charcount_t count;  // Total number of elements, in all chunks.
        charcount_t firstChunkLength;
        charcount_t initialSize;

        bool IsChained() { return this->secondChunk != NULL; }

        Data *NewChainedChunk(charcount_t bufLengthRequested)
        {
            CompileAssert(sizeof(charcount_t) == sizeof(uint32));

            // allocation = (bufLengthRequested * sizeof(wchar_t) + sizeof(Data)
            charcount_t alloc32 = UInt32Math::MulAdd<sizeof(wchar_t), sizeof(Data)>(bufLengthRequested);
            size_t allocation = TAllocator::GetAlignedSize(alloc32);
            size_t size_t_length = (allocation - sizeof(Data)) / sizeof(wchar_t);
            charcount_t bufLength = (charcount_t)size_t_length;
            Assert(bufLength == size_t_length);

            Data *newChunk = AllocatorNewStructPlus(TAllocator, this->alloc, allocation, Data);

            newChunk->u.chained.length = bufLength;
            newChunk->u.chained.next = NULL;

            // Recycler gives zeroed memory, so rely on that instead of memsetting the tail
#if 0
            // Align memset to machine register size for perf
            bufLengthRequested &= ~(sizeof(size_t) - 1);
            memset(newChunk->u.chained.buffer + bufLengthRequested, 0, (bufLength - bufLengthRequested) * sizeof(wchar_t));
#endif
            return newChunk;
        }

        Data *NewSingleChunk(charcount_t *pBufLengthRequested)
        {
            Assert(*pBufLengthRequested <= MaxLength);

            // Let's just grow the current chunk in place

            CompileAssert(sizeof(charcount_t) == sizeof(uint32));

            //// allocation = (bufLengthRequested+1) * sizeof(wchar_t)
            charcount_t alloc32 = UInt32Math::AddMul< 1, sizeof(wchar_t) >(*pBufLengthRequested);

            size_t allocation = HeapInfo::GetAlignedSize(alloc32);
            size_t size_t_newLength  = allocation / sizeof(wchar_t) - 1;
            charcount_t newLength = (charcount_t)size_t_newLength;
            Assert(newLength == size_t_newLength);
            Assert(newLength <= MaxLength + 1);
            if (newLength == MaxLength + 1)
            {
                // newLength could be MaxLength + 1 because of alignment.
                // In this case alloc size is 2 elements more than newLength (normally 1 elements more for NULL), that's fine.
                newLength = MaxLength;
            }
            Assert(newLength <= MaxLength);

            Data* newChunk = AllocatorNewStructPlus(TAllocator, this->alloc, allocation, Data);
            newChunk->u.single.buffer[newLength] = L'\0';

            *pBufLengthRequested = newLength;
            return newChunk;
        }

        __declspec(noinline) void ExtendBuffer(charcount_t newLength)
        {
            Data *newChunk;

            // To maintain this->length under MaxLength, check it here/throw, this is the only place we grow the buffer.
            if (newLength > MaxLength)
            {
                Throw::OutOfMemory();
            }

            Assert(this->length <= MaxLength);

            charcount_t newLengthTryGrowPolicy = newLength + (this->length*2/3); // Note: this would never result in uint32 overflow.
            if (newLengthTryGrowPolicy <= MaxLength)
            {
                newLength = newLengthTryGrowPolicy;
            }
            Assert(newLength <= MaxLength);

            // We already have linked chunks
            if (this->IsChained() || (this->firstChunk != NULL && newLength - this->length > MaxRealloc))
            {
                newChunk = this->NewChainedChunk(newLength - this->count);

                if (this->IsChained())
                {
                    this->lastChunk->u.chained.next = newChunk;

                    // We're not going to use the extra space in the current chunk...
                    Assert(this->lastChunk->u.chained.length > this->length - this->count);
                    this->lastChunk->u.chained.length -= (this->length - this->count);
                }
                else
                {
                    // Time to add our first linked chunk
                    Assert(this->secondChunk == NULL);
                    this->secondChunk = newChunk;

                    // We're not going to use the extra space in the current chunk...
                    this->firstChunkLength = this->count;
                }

                this->length = this->count + newChunk->u.chained.length;
                this->lastChunk = newChunk;
                this->appendPtr = newChunk->u.chained.buffer;
            }
            else
            {
                if (this->initialSize < MaxLength)
                {
                    newLength = max(newLength, this->initialSize + 1);
                }
                else
                {
                    newLength = MaxLength;
                }
                Assert(newLength <= MaxLength);

                // Let's just grow the current chunk in place
                newChunk = this->NewSingleChunk(&newLength);

                if (this->count)
                {
                    js_memcpy_s(newChunk->u.single.buffer, newLength * sizeof(wchar_t), this->firstChunk->u.single.buffer, sizeof(wchar_t) * this->count);
                }

                this->firstChunk = this->lastChunk = newChunk;
                this->firstChunkLength = newLength;
                this->length = newLength;
                this->appendPtr = newChunk->u.single.buffer + this->count;
            }
        }

        void EnsureBuffer(charcount_t countNeeded)
        {
            if(countNeeded == 0) return;

            if (countNeeded >= this->length - this->count)
            {
                if (countNeeded > MaxLength)
                {
                    // Check upfront to prevent potential uint32 overflow caused by (this->count + countNeeded + 1).
                    Throw::OutOfMemory();
                }
                ExtendBuffer(this->count + countNeeded + 1);
            }
        }

    public:

        static StringBuilder<TAllocator> *
            New(TAllocator* alloc, charcount_t initialSize)
        {
            if (initialSize > MaxLength)
            {
                Throw::OutOfMemory();
            }
            return AllocatorNew(TAllocator, alloc, StringBuilder<TAllocator>, alloc, initialSize);
        }

        StringBuilder(TAllocator* alloc)
        {
            new (this) StringBuilder(alloc, 0);
        }

        StringBuilder(TAllocator* alloc, charcount_t initialSize) : alloc(alloc), length(0), count(0), firstChunk(NULL),
            secondChunk(NULL), appendPtr(NULL), initialSize(initialSize)
        {
            if (initialSize > MaxLength)
            {
                Throw::OutOfMemory();
            }
        }

        void UnChain(__out __ecount(bufLen) wchar_t *pBuf, charcount_t bufLen)
        {
            charcount_t lastChunkCount = this->count;

            Assert(this->IsChained());

            Assert(bufLen >= this->count);
            wchar_t *pSrcBuf = this->firstChunk->u.single.buffer;
            Data *next = this->secondChunk;
            charcount_t srcLength = this->firstChunkLength;

            for (Data *chunk = this->firstChunk; chunk != this->lastChunk; next = chunk->u.chained.next)
            {
                if (bufLen < srcLength)
                {
                    Throw::FatalInternalError();
                }
                js_memcpy_s(pBuf, bufLen * sizeof(wchar_t), pSrcBuf, sizeof(wchar_t) * srcLength);
                bufLen -= srcLength;
                pBuf += srcLength;
                lastChunkCount -= srcLength;

                chunk = next;
                pSrcBuf = chunk->u.chained.buffer;
                srcLength = chunk->u.chained.length;
            }

            if (bufLen < lastChunkCount)
            {
                Throw::FatalInternalError();
            }
            js_memcpy_s(pBuf, bufLen * sizeof(wchar_t), this->lastChunk->u.chained.buffer, sizeof(wchar_t) * lastChunkCount);
        }

        void UnChain()
        {
            Assert(this->IsChained());

            charcount_t newLength = this->count;

            Data *newChunk = this->NewSingleChunk(&newLength);

            this->length = newLength;

            this->UnChain(newChunk->u.single.buffer, newLength);

            this->firstChunk = this->lastChunk = newChunk;
            this->secondChunk = NULL;
            this->appendPtr = newChunk->u.single.buffer + this->count;
        }

        void Copy(__out __ecount(bufLen) wchar_t *pBuf, charcount_t bufLen)
        {
            if (this->IsChained())
            {
                this->UnChain(pBuf, bufLen);
            }
            else
            {
                if (bufLen < this->count)
                {
                    Throw::FatalInternalError();
                }
                js_memcpy_s(pBuf, bufLen * sizeof(wchar_t), this->firstChunk->u.single.buffer, this->count * sizeof(wchar_t));
            }
        }

        inline wchar_t* Buffer()
        {
            if (this->IsChained())
            {
                this->UnChain();
            }

            if (this->firstChunk)
            {
                this->firstChunk->u.single.buffer[this->count] = L'\0';

                return this->firstChunk->u.single.buffer;
            }
            else
            {
                return L"";
            }
        }

        inline charcount_t Count() { return this->count; }

        void Append(wchar_t c)
        {
            if (this->count == this->length)
            {
                ExtendBuffer(this->length+1);
            }
            *(this->appendPtr++) = c;
            this->count++;
        }

        void AppendSz(const wchar_t * str)
        {
            // WARNING!!
            // Do not use this to append JavascriptStrings.  They can have embedded
            // nulls which obviously won't be handled correctly here.  Instead use
            // Append with a length, which will use memcpy and correctly include any
            // embedded null characters.
            // WARNING!!

            while (*str != L'\0')
            {
                Append(*str++);
            }
        }

        void Append(const wchar_t * str, charcount_t countNeeded)
        {
            EnsureBuffer(countNeeded);

            wchar_t *dst = this->appendPtr;

            JavascriptString::CopyHelper(dst, str, countNeeded);

            this->appendPtr += countNeeded;
            this->count += countNeeded;
        }

        template <size_t N>
        void AppendCppLiteral(const wchar_t(&str)[N])
        {
            // Need to account for the terminating null character in C++ string literals, hence N > 2 and N - 1 below
            static_assert(N > 2, "Use Append(wchar_t) for appending literal single characters and do not append empty string literal");
            Append(str, N - 1);
        }

        // If we expect str to be large - we should just use this version that uses memcpy directly instead of Append
        void AppendLarge(const wchar_t * str, charcount_t countNeeded)
        {
            EnsureBuffer(countNeeded);

            wchar_t *dst = this->appendPtr;

            js_memcpy_s(dst, sizeof(WCHAR) * countNeeded, str, sizeof(WCHAR) * countNeeded);

            this->appendPtr += countNeeded;
            this->count += countNeeded;
        }


        errno_t AppendUint64(unsigned __int64 value)
        {
            const int max_length = 20; // maximum length of 64-bit value converted to base 10 string
            const int radix = 10;
            WCHAR buf[max_length+1];
            errno_t result = _ui64tow_s(value, buf, max_length+1, radix);
            AssertMsg(result==0, "Failed to translate value to string");
            if (result == 0)
            {
                AppendSz(buf);
            }
            return result;
        }

        wchar_t *AllocBufferSpace(charcount_t countNeeded)
        {
            EnsureBuffer(countNeeded);

            return this->appendPtr;
        }

        void IncreaseCount(charcount_t countInc)
        {
            if(countInc == 0) return;

            this->count += countInc;
            this->appendPtr += countInc;
            Assert(this->count < this->length);
        }

        wchar_t* Detach()
        {
            // NULL terminate the string
            Append(L'\0');

            // if there is a chain we need to account for that also, so that the new buffer will have the NULL at the end.
            if (this->IsChained())
            {
                this->UnChain();
            }
            // Now decrement the count to adjust according to number of chars
            this->count--;

            wchar_t* result = this->firstChunk->u.single.buffer;
            this->firstChunk = this->lastChunk = NULL;
            return result;
        }

        void TrimTrailingNULL()
        {
            Assert(this->count);
            if (this->IsChained())
            {
                Assert(this->lastChunk->u.chained.buffer[this->count - (this->length - this->lastChunk->u.chained.length) - 1] == L'\0');
            }
            else
            {
                Assert(this->lastChunk->u.single.buffer[this->count - 1] == L'\0');
            }
            this->appendPtr--;
            this->count--;
        }

        void Reset()
        {
            this->length = 0;
            this->count = 0;
            this->firstChunk = NULL;
            this->secondChunk = NULL;
            this->lastChunk = NULL;
        }
    };
}
