// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

var o = { "foo": 42, "bar": 32 };

function foo(o, k, do_lookup) {
  if (do_lookup)
    return o[k];
}

%PrepareFunctionForOptimization(foo);
// Warm up the keyed load feedback in foo as always loading the name "foo".
assertEquals(42, foo(o, "foo", true));
assertEquals(42, foo(o, "foo", true));

function bar(o, do_lookup) {
  // Bar does a lookup with "bar" on the object, using foo.
  return foo(o, "bar", do_lookup);
}
%PrepareFunctionForOptimization(bar);
// Warm up bar as never having done the lookup, so that the named feedback in
// foo stays "foo".
assertEquals(undefined, bar(o, false));
assertEquals(undefined, bar(o, false));

// This optimization should not be confused by the potential mismatch between
// the "foo" named feedback in foo, and the "bar" constant string in bar.
%OptimizeMaglevOnNextCall(bar);
assertEquals(undefined, bar(o, false));
assertEquals(32, bar(o, true));
