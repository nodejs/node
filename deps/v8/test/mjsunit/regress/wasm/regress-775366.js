// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');

(function BadTypeSection() {
  var data = bytes(
    kWasmH0,
    kWasmH1,
    kWasmH2,
    kWasmH3,

    kWasmV0,
    kWasmV1,
    kWasmV2,
    kWasmV3,

    kTypeSectionCode,
    5,
    2,
    0x60,
    0,
    0,
    13
  );

  assertFalse(WebAssembly.validate(data));
})();
