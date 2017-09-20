// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FIND_AND_REPLACE_PATTERN_H_
#define V8_FIND_AND_REPLACE_PATTERN_H_

#include "src/handles.h"

namespace v8 {
namespace internal {

class Map;
class Object;

class FindAndReplacePattern {
 public:
  FindAndReplacePattern() : count_(0) {}
  void Add(Handle<Map> map_to_find, Handle<HeapObject> obj_to_replace) {
    DCHECK(count_ < kMaxCount);
    find_[count_] = map_to_find;
    replace_[count_] = obj_to_replace;
    ++count_;
  }

 private:
  static const int kMaxCount = 4;
  int count_;
  Handle<Map> find_[kMaxCount];
  Handle<HeapObject> replace_[kMaxCount];
  friend class Code;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FIND_AND_REPLACE_PATTERN_H_
