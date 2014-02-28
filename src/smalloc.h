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

// Forward declaration
class Environment;

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
 * Return byte size of external array type.
 */
NODE_EXTERN size_t ExternalArraySize(enum v8::ExternalArrayType type);

/**
 * Allocate external array data onto obj.
 *
 * Passed data transfers ownership, and if no callback is passed then memory
 * will automatically be free'd using free() (not delete[]).
 *
 * length is always the byte size of the data. Not the length of the external
 * array. This intentionally differs from the JS API so users always know
 * exactly how much memory is being allocated, regardless of the external array
 * type. For this reason the helper function ExternalArraySize is provided to
 * help determine the appropriate byte size to be allocated.
 *
 * In the following example we're allocating a Float array and setting the
 * "length" property on the Object:
 *
 * \code
 *    size_t array_length = 8;
 *    size_t byte_length = node::smalloc::ExternalArraySize(
 *        v8::kExternalFloatArray);
 *    v8::Local<v8::Object> obj = v8::Object::New();
 *    char* data = static_cast<char*>(malloc(byte_length * array_length));
 *    node::smalloc::Alloc(obj, data, byte_length, v8::kExternalFloatArray);
 *    obj->Set(v8::String::NewFromUtf8("length"),
 *             v8::Integer::NewFromUnsigned(array_length));
 * \code
 */
NODE_EXTERN void Alloc(Environment* env,
                       v8::Handle<v8::Object> obj,
                       size_t length,
                       enum v8::ExternalArrayType type =
                       v8::kExternalUnsignedByteArray);
NODE_EXTERN void Alloc(Environment* env,
                       v8::Handle<v8::Object> obj,
                       char* data,
                       size_t length,
                       enum v8::ExternalArrayType type =
                       v8::kExternalUnsignedByteArray);
NODE_EXTERN void Alloc(Environment* env,
                       v8::Handle<v8::Object> obj,
                       size_t length,
                       FreeCallback fn,
                       void* hint,
                       enum v8::ExternalArrayType type =
                       v8::kExternalUnsignedByteArray);
NODE_EXTERN void Alloc(Environment* env,
                       v8::Handle<v8::Object> obj,
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
NODE_EXTERN void AllocDispose(Environment* env, v8::Handle<v8::Object> obj);


/**
 * Check if the Object has externally allocated memory.
 */
NODE_EXTERN bool HasExternalData(Environment* env, v8::Local<v8::Object> obj);

}  // namespace smalloc
}  // namespace node

#endif  // SRC_SMALLOC_H_
