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

// Recent 4.x MinGW and MinGW-w64 gcc compilers lack std::mutex and
// std::lock_guard.  I have a 5.2.0 MinGW-w64 compiler packaged through MSYS2
// that *is* new enough, but that's one compiler against several deficient
// ones.  Wrap CRITICAL_SECTION instead.

#ifndef WINPTY_SHARED_MUTEX_H
#define WINPTY_SHARED_MUTEX_H

#include <windows.h>

class Mutex {
    CRITICAL_SECTION m_mutex;
public:
    Mutex()         { InitializeCriticalSection(&m_mutex);  }
    ~Mutex()        { DeleteCriticalSection(&m_mutex);      }
    void lock()     { EnterCriticalSection(&m_mutex);       }
    void unlock()   { LeaveCriticalSection(&m_mutex);       }

    Mutex(const Mutex &other) = delete;
    Mutex &operator=(const Mutex &other) = delete;
};

template <typename T>
class LockGuard {
    T &m_lock;
public:
    LockGuard(T &lock) : m_lock(lock)  { m_lock.lock();    }
    ~LockGuard()                       { m_lock.unlock();  }

    LockGuard(const LockGuard &other) = delete;
    LockGuard &operator=(const LockGuard &other) = delete;
};

#endif // WINPTY_SHARED_MUTEX_H
