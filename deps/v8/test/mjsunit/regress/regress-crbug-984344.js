// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function largeAllocToTriggerGC() {
  for (let i = 0; i < 16; i++) {
    let ab = new ArrayBuffer(1024 * 1024 * 10);
  }
}

function foo() {
  eval('function bar(a) {}' +
       '(function() {' +
       '  for (let c = 0; c < 505; c++) {' +
       '    while (Promise >= 0xDEADBEEF) {' +
       '      Array.prototype.slice.call(bar, bar, bar);' +
       '    }' +
       '    for (let i = 0; i < 413; i++) {' +
       '    }' +
       '  }' +
       '})();' +
       'largeAllocToTriggerGC();');
}


foo();
foo();
foo();
// Don't prepare until here to allow function to be flushed.
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo();
