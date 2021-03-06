// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let v0 = [0, 1];
v0.constructor = {
  [Symbol.species]: function() {
    let v1 = [2];
    Object.defineProperty(v1, "length", {writable: false});
    return v1;
  }
}

assertThrows(() => Array.prototype.map.call(v0, function() {}), TypeError);
