// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function main() {
  // This isn't really a Wasm-related test (so doesn't belong in regress/wasm/),
  // but it does use WebAssembly.instantiate to trigger the original issue.
  if (typeof WebAssembly === 'undefined') return;

  Object.defineProperty(Promise, Symbol.species, {
    value: function (f) {
      f(() => { throw 111}, () => { throw 222});
    }
  });
  const promise = WebAssembly.instantiate(new ArrayBuffer(0x10));
  promise.then();
}
main();
