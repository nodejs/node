// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strict-termination-checks --ignore-unhandled-promises

let resolve;
globalThis.p = new Promise(r => { resolve = r; });

const src = "import 'data:text/javascript, await globalThis.p'; await 0";
import('data:text/javascript,' + src);

setTimeout(() => {
  Object.defineProperty(Promise.prototype, 'constructor', {
    get() { throw 42; }
  });
  resolve();
});
