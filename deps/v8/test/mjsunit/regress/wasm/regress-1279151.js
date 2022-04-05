// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

// --experimental-wasm-gc should not affect asm-js modules.

function NewModule() {
  "use asm";
  function foo() {}
  return {foo:foo};
};

var v = NewModule();
