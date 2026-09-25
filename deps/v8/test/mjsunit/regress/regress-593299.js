// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

function h(global) { return global.boom(); }
function g() { var r = h({}); return r; }
function f() {
  var o = {};
  o.__defineGetter__('prop1', g);
  o.prop1;
}

assertThrows(f);
