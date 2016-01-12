// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef DEFTYPE
#define DEFTYPE(T)
#endif

#ifndef DEFMETHOD
#define DEFMETHOD(T, M)
#endif

// These global constructors will be cached
DEFTYPE(ArrayBuffer)
DEFTYPE(Boolean)
DEFTYPE(Date)
DEFTYPE(Error)
DEFTYPE(EvalError)
DEFTYPE(Function)
DEFTYPE(Number)
DEFTYPE(Object)
DEFTYPE(Proxy)
DEFTYPE(RangeError)
DEFTYPE(ReferenceError)
DEFTYPE(RegExp)
DEFTYPE(String)
DEFTYPE(SyntaxError)
DEFTYPE(TypeError)
DEFTYPE(Uint8Array)
DEFTYPE(Uint8ClampedArray)
DEFTYPE(Int8Array)
DEFTYPE(Uint16Array)
DEFTYPE(Int16Array)
DEFTYPE(Uint32Array)
DEFTYPE(Int32Array)
DEFTYPE(Float32Array)
DEFTYPE(Float64Array)
DEFTYPE(URIError)
DEFTYPE(Promise)
DEFTYPE(Map)
DEFTYPE(Set)


// These prototype functions will be cached/shimmed
DEFMETHOD(Object,         hasOwnProperty)
DEFMETHOD(Object,         toString)
DEFMETHOD(String,         concat)

#undef DEFTYPE
#undef DEFMETHOD
