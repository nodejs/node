// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __f_3(__v_212, __v_213) {
  var __v_214 = Object.getOwnPropertyNames(__v_212);
  if (__v_214.includes() && __v_17.constructor.hasOwnProperty()) {
  }
  return __v_214[__v_213 % __v_214.length];
}
var __v_239 = {element: 'anyfunc', initial: 10};
__v_239.__defineGetter__(__f_3(__v_239, 1603979645), function() {
  return __f_10();
});

assertThrows(() => new WebAssembly.Table(__v_239));
