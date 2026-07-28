// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --ignore-unhandled-promises

Object.defineProperty(Promise.prototype, 'constructor', {
  get() { throw 42; }
});

(async () => {
  const src = `const p = new Promise(r => {}); await p;`;
  await import(`data:text/javascript, ${src}`);
})();
