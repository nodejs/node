// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// The localeCompare inlined fast path (StringLocaleCompareIntl) bails to the
// kStringFastLocaleCompare builtin, which in turn can call the user-visible
// localeCompare and trigger ToString on its argument. The Turbolev graph
// builder must wire that node into the surrounding catch handler, otherwise
// the throw escapes the try/catch.

var val = [];
Object.defineProperty(val, "join", {
  get: function () { throw "boom"; }
});

function foo() {
  try {
    "en-US".localeCompare(val);
  } catch (e) {}
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
