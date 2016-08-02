// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_URI_H_
#define V8_URI_H_

#include "src/allocation.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class Uri : public AllStatic {
 public:
  static Object* EncodeUri(Isolate* isolate, Handle<String> uri) {
    return Encode(isolate, uri, true);
  }

  static Object* EncodeUriComponent(Isolate* isolate,
                                    Handle<String> component) {
    return Encode(isolate, component, false);
  }

  // DecodeUri
  // DecodeUriComponent
  // escape
  // unescape

 private:
  static Object* Encode(Isolate* isolate, Handle<String> uri, bool is_uri);
  // decode
};

}  // namespace internal
}  // namespace v8

#endif  // V8_URI_H_
