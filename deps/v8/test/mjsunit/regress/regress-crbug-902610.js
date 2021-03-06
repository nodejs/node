// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => {
  // Make a function with 65535 args. This should throw a SyntaxError because -1
  // is reserved for the "don't adapt arguments" sentinel.
  var f_with_65535_args =
      eval("(function(" + Array(65535).fill("x").join(",") + "){})");
  f_with_65535_args();
}, SyntaxError);
