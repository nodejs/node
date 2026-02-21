// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = [0, 1];
a.constructor = {
  [Symbol.species]: function(len) {
    var arr = {length: 0};
    Object.defineProperty(arr, "length", {writable: false});
    return arr;
  }
}
assertThrows(() => { Array.prototype.concat.call(a) }, TypeError);
