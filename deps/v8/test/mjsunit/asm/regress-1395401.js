// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

eval(`
    function asmModule() {
      "use asm";
      function x(v) {
        v = v | 0;
      }
      return x;
    }
    asmModule();
  `);

console.profile();
