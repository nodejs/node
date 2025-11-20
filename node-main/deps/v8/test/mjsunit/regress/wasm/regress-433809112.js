// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function module() {
  "use asm";
  function f() {}
  return {f: f};
}
Object.defineProperty(WebAssembly.Instance.prototype, '__single_function__', {
  get: () => {
    throw null;
  }
});
module();
