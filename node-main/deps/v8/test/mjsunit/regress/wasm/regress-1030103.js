// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --log-code

(function(foo, foreign) {
  'use asm';
  var fn = foreign.fn;
  function f() { }
  return f;
})(this, {fn: x => x});
