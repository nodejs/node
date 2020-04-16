// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking
// Flags: --no-stress-opt

// Since cleanup tasks are top-level tasks, errors thrown from them don't stop
// future cleanup tasks from running.

function callback(iter) {
  [...iter];
  throw new Error('callback');
};

const fg1 = new FinalizationGroup(callback);
const fg2 = new FinalizationGroup(callback);

(function() {
let x = {};
fg1.register(x, {});
fg2.register(x, {});
x = null;
})();

gc();
