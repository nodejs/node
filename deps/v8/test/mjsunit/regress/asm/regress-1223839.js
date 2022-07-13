// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --print-code

function module(global, imports) {
  'use asm';
  var x = imports.fn;
  function f() {}
  return f;
}

module(this, {fn: i => i});
