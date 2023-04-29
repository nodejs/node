// Copyright (c) 2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef SIMPLE_POOL_H
#define SIMPLE_POOL_H

#include <stdlib.h>

#include <vector>

#include "../shared/WinptyAssert.h"

template <typename T, size_t chunkSize>
class SimplePool {
public:
    ~SimplePool();
    T *alloc();
    void clear();
private:
    struct Chunk {
        size_t count;
        T *data;
    };
    std::vector<Chunk> m_chunks;
};

template <typename T, size_t chunkSize>
SimplePool<T, chunkSize>::~SimplePool() {
    clear();
}

template <typename T, size_t chunkSize>
void SimplePool<T, chunkSize>::clear() {
    for (size_t ci = 0; ci < m_chunks.size(); ++ci) {
        Chunk &chunk = m_chunks[ci];
        for (size_t ti = 0; ti < chunk.count; ++ti) {
            chunk.data[ti].~T();
        }
        free(chunk.data);
    }
    m_chunks.clear();
}

template <typename T, size_t chunkSize>
T *SimplePool<T, chunkSize>::alloc() {
    if (m_chunks.empty() || m_chunks.back().count == chunkSize) {
        T *newData = reinterpret_cast<T*>(malloc(sizeof(T) * chunkSize));
        ASSERT(newData != NULL);
        Chunk newChunk = { 0, newData };
        m_chunks.push_back(newChunk);
    }
    Chunk &chunk = m_chunks.back();
    T *ret = &chunk.data[chunk.count++];
    new (ret) T();
    return ret;
}

#endif // SIMPLE_POOL_H
