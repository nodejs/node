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

#include "v8.h"

namespace node {

class SlabAllocator {
public:
  SlabAllocator(unsigned int size = 10485760); // default to 10M
  ~SlabAllocator();

  // allocate memory from slab, attaches the slice to `obj`
  char* Allocate(v8::Handle<v8::Object> obj, unsigned int size);

  // return excess memory to the slab, returns a handle to the parent buffer
  v8::Local<v8::Object> Shrink(v8::Handle<v8::Object> obj,
                               char* ptr,
                               unsigned int size);

private:
  void Initialize();
  bool initialized_;
  v8::Persistent<v8::Object> slab_;
  v8::Persistent<v8::String> slab_sym_;
  unsigned int offset_;
  unsigned int size_;
  char* last_ptr_;
};

} // namespace node
