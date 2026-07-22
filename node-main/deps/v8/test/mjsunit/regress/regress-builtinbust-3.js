// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function produce_object() {
  var real_length = 1;
  function set_length() { real_length = "boom"; }
  function get_length() { return real_length; }
  var o = { __proto__:Array.prototype , 0:"x" };
  Object.defineProperty(o, "length", { set:set_length, get:get_length })
  return o;
}

assertEquals(2, produce_object().push("y"));
assertEquals(2, produce_object().unshift("y"));
