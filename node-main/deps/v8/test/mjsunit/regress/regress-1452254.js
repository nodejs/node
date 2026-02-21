// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

let o = { toJSON() {} };
function ID(x) { return x; }

class C0 {
  toJSON() {}
};

class C1 {
  toJSON() {}
  [ID('x')](){}
};

class C2 {
  static toJSON() {}
};

class C3 {
  static toJSON() {}
  static [ID('x')](){}
};
