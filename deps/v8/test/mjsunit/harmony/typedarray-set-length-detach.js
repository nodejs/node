// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


let ta = new Int32Array(10);
assertThrows(() => {
  ta.set({
    get length() {
      %ArrayBufferDetach(ta.buffer);
      return 1;
    },
    get 0() {
      return 100;
    },
  });
}, TypeError);
