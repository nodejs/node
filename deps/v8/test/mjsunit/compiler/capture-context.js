// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var foo = (function() {
  var x = 42;
  function bar(s) { return x + s; }
  return (function (s) { return bar(s); })
})();

var baz = (function (s) { return foo(s) });

%OptimizeFunctionOnNextCall(baz);
assertEquals(42 + 12, baz(12));
