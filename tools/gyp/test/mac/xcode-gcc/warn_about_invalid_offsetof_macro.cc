// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define offsetof(st, m) ((unsigned)((char*)&((st*)0)->m - (char*)0))

struct MyStruct {
  virtual void MyFunc() = 0;
  int my_member;
};

int main() {
  unsigned x = offsetof(MyStruct, my_member);
  return x ? 0 : 1;
}
