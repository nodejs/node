// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ab = new Int8Array(20).map((v, i) => i).buffer;
var ta = new Int8Array(ab, 0, 10);
var seen_length = -1;
ta.constructor = {
  [Symbol.species]: function(len) {
    seen_length = len;
    return new Int8Array(ab, 1, len);
  }
};

assertEquals(-1, seen_length);
assertArrayEquals([0,1,2,3,4,5,6,7,8,9], ta);
var tb = ta.slice();
assertEquals(10, seen_length);
assertArrayEquals([0,0,0,0,0,0,0,0,0,0], ta);
assertArrayEquals([0,0,0,0,0,0,0,0,0,0], tb);
