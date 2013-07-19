// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_SMALLOC_H_
#define SRC_SMALLOC_H_

#include "node.h"
#include "v8.h"

namespace node {

/**
 * Simple memory allocator.
 *
 * Utilities for external memory allocation management. Is an abstraction for
 * v8's external array data handling to simplify and centralize how this is
 * managed.
 */
namespace smalloc {

// mirrors deps/v8/src/objects.h
static const unsigned int kMaxLength = 0x3fffffff;

NODE_EXTERN typedef void (*FreeCallback)(char* data, void* hint);

/**
 * Allocate external memory and set to passed object. If data is passed then
 * will use that instead of allocating new.
 *
 * When you pass an ExternalArrayType and data, Alloc assumes data length is
 * the same as data length * ExternalArrayType length.
 */
NODE_EXTERN void Alloc(v8::Handle<v8::Object> obj,
                       size_t length,
                       enum v8::ExternalArrayType type =
                       v8::kExternalUnsignedByteArray);
NODE_EXTERN void Alloc(v8::Handle<v8::Object> obj,
                       char* data,
                       size_t length,
                       enum v8::ExternalArrayType type =
                       v8::kExternalUnsignedByteArray);
NODE_EXTERN void Alloc(v8::Handle<v8::Object> obj,
                       size_t length,
                       FreeCallback fn,
                       void* hint,
                       enum v8::ExternalArrayType type =
                       v8::kExternalUnsignedByteArray);
NODE_EXTERN void Alloc(v8::Handle<v8::Object> obj,
                       char* data,
                       size_t length,
                       FreeCallback fn,
                       void* hint,
                       enum v8::ExternalArrayType type =
                       v8::kExternalUnsignedByteArray);

/**
 * Free memory associated with an externally allocated object. If no external
 * memory is allocated to the object then nothing will happen.
 */
NODE_EXTERN void AllocDispose(v8::Handle<v8::Object> obj);

}  // namespace smalloc
}  // namespace node

#endif  // SRC_SMALLOC_H_
