// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

let arr = [];
class Base {
  x = arr.push();
}

class Child extends Base {
  constructor() {
    arr = () => {
      try { arr(); } catch {  /* max call stack size error */ }
      super();  // arr.push called after super() -> non callable error
    };
    arr();
  }
}

assertThrows(() => new Child(), TypeError, /arr.push is not a function/);
