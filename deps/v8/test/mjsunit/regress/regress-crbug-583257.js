// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(String.prototype, "0", { __v_1: 1});
Object.defineProperty(String.prototype, "3", { __v_1: 1});

(function () {
  var s = new String();
  function set(object, index, value) { object[index] = value; }
  set(s, 10, "value");
  set(s, 1073741823, "value");
})();

function __f_11() {
  Object.preventExtensions(new String());
}
__f_11();
__f_11();

(function() {
  var i = 10;
  var a = new String("foo");
  for (var j = 0; j < i; j++) {
    a[j] = {};
  }
})();
