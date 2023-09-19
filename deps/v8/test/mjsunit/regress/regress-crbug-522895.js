// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

var body =
  "function bar1(  )  {" +
  "  var i = 35;       " +
  "  while (i-- > 31) {" +
  "    %OptimizeOsr(); " +
  "    j = 9;          " +
  "    %PrepareFunctionForOptimization(bar1); " +
  "    while (j-- > 7);" +
  "  }                 " +
  "  return i;         " +
  "}";

function gen() {
  return eval("(" + body + ")");
}

var f = gen();
%PrepareFunctionForOptimization(f);
f();
