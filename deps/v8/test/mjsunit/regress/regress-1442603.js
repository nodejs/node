// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code
// Flags: --always-turbofan --always-osr --osr-to-tierup=200

function main() {
  function f0() {
    try {
      var v0 = 0;
    } catch (v3) { }
    try {
      var v1 = new Array(23);
    } catch (v4) { }
    try {
      for (; v0 < v1.length; v0++) {
        v1[v0] = new Uint32Array(262142);
        v1[v0] = new Uint32Array(262139);
        v1[v0] = new Uint32Array(262139);
      }
    } catch (v5) { }
    v0 = -13;
    try {
      var v2 = [];
    } catch (v6) { }
    try {
      var v1 = new Array(23);
    } catch (v7) { }
    try {
      v2.xxx = "xxx";
    } catch (e) { }
    try {
      for (var v0 = 0; v0 < 1024; v0++) {
        v2[v0] = new Array(v0);
        v2[v0] = new Array(v0);
        v2[v0] = new Array(v0);
        v2[v0].xxx = "xxx " + v0;
        v2[v0].xxx = "xxx " + v0;
        v2[v0].xxx = "xxx " + v0;
      }
    } catch (v8) { }
    try {
      gc();
      a(900000)[b(b(900000), 900000)] = 900000;
    } catch (v9) { }
  }
  gc();
  f0();
}
main();
main();
main();
