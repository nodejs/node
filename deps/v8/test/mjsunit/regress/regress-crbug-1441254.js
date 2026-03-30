// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-lazy-compilation=100 --throws

var counter = 0;

let k = 0;
for (k = 0; k < 50; k++) {
  let src = `
    function g() {
      var C = class C${k} {
        static sx = counter++;
        x = counter++;
        static {
          counter++;
        }
        #y = counter++;
        z = counter++;
      }
      return C;
    }
  `;
  eval(src);
  for (let j = 0; j < 5; j++) {
    try {
      var C = g();
    } catch(e) {
      // Ignore potential stack overflows.
    }
  }
  new C();
}

throw Error("success");
