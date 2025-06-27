// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

// Since cleanup tasks are top-level tasks, errors thrown from them don't stop
// future cleanup tasks from running.

function callback(holdings) {
  throw new Error('callback');
};

const fg1 = new FinalizationRegistry(callback);
const fg2 = new FinalizationRegistry(callback);

(function() {
  let x = {};
  fg1.register(x, {});
  fg2.register(x, {});
  x = null;
})();

// We need to invoke GC asynchronously and wait for it to finish, so that
// it doesn't need to scan the stack. Otherwise, the objects may not be
// reclaimed because of conservative stack scanning and the test may not
// work as intended.
(async function () {
  await gc({ type: 'major', execution: 'async' });
})();
