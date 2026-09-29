// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --single-threaded --no-lazy-feedback-allocation

function foo() {
  const v1 = ("string").normalize();
  const v4 = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([
      0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
  ])));
  const v5 = v4.exports;
  for (let v6 = 0; v6 < 5; v6++) {
      for (let i8 = Int32Array;
          (() => {
              function f9(a10, a11 = i8) {
                  const v12 = a11 instanceof i8;
                  return Uint8Array & v5;
              }
              try { const v14 = f9(); } catch (e) {}
              function f16(a17, a18, a19, a20) {
                  return v6;
              }
              try { const v21 = v1(); } catch (e) {}
              return 0;
          })();
          ) {
      }
      %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(foo);
foo();
