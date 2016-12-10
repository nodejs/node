// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

// Ensure checked in wasm binaries used by integration tests from v8 hosts
// (such as chromium) are up to date.

(function ensure_incrementer() {
  var buff = readbuffer("test/mjsunit/wasm/incrementer.wasm");
  var mod = new WebAssembly.Module(buff);
  var inst = new WebAssembly.Instance(mod);
  var inc = inst.exports.increment;
  assertEquals(3, inc(2));
}())
