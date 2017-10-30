// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sym = Symbol();
function asm(stdlib, ffi) {
  "use asm";
  var get_sym = ffi.get_sym;
  function crash() {
    get_sym()|0;
  }
  return {crash: crash};
}
function get_sym() {
  return sym;
}
try {
  asm(null, {get_sym: get_sym}).crash();
} catch (e) {
  if (!(e instanceof TypeError))
    throw e;
}
