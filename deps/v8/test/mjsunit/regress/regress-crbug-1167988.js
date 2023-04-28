// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let o1 = {
  [() => {}]() {
    return super.m();
  }
};

let o2 = {
  get [() => {}]() {
    return super.m();
  }
};

let o3 = {
  [() => {}]: 1,
  m2() { super.x; }
};
