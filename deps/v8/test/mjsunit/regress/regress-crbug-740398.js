// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var longString = (function() {
  var str = "";
  for (var i = 0; i < 24; i++) {
    str += "abcdefgh12345678" + str;
  }
  return str;
})();

assertThrows(() => { return { get[longString]() { } } }, RangeError);
assertThrows(() => { return { set[longString](v) { } } }, RangeError);
assertThrows(() => { return { [Symbol(longString)]: () => {} } }, RangeError);
