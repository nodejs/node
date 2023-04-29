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

// Convenience header library for using the high-resolution performance counter
// to measure how long some process takes.

#ifndef TIME_MEASUREMENT_H
#define TIME_MEASUREMENT_H

#include <windows.h>
#include <assert.h>
#include <stdint.h>

class TimeMeasurement {
public:
    TimeMeasurement() {
        static double freq = static_cast<double>(getFrequency());
        m_freq = freq;
        m_start = value();
    }

    double elapsed() {
        uint64_t elapsedTicks = value() - m_start;
        return static_cast<double>(elapsedTicks) / m_freq;
    }

private:
    uint64_t getFrequency() {
        LARGE_INTEGER freq;
        BOOL success = QueryPerformanceFrequency(&freq);
        assert(success && "QueryPerformanceFrequency failed");
        return freq.QuadPart;
    }

    uint64_t value() {
        LARGE_INTEGER ret;
        BOOL success = QueryPerformanceCounter(&ret);
        assert(success && "QueryPerformanceCounter failed");
        return ret.QuadPart;
    }

    uint64_t m_start;
    double m_freq;
};

#endif // TIME_MEASUREMENT_H
