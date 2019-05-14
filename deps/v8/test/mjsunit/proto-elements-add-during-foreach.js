// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(()=>{
  var a = [0,1,2,,,,7];
  var proto = {}
  a.__proto__ = proto;
  var visits = 0;
  Array.prototype.forEach.call(a, (v,i,o) => { ++visits; proto[4] = 4; });
  assertEquals(5, visits);
})();

// We have a fast path for arrays with the initial array prototype.
// If elements are inserted into the initial array prototype as we traverse
// a holey array, we should gracefully exit the fast path.
(()=>{
  let a = [1, 2, 3,,,, 7];
  function poison(v, i) {
    if (i === 2) {
      [].__proto__[4] = 3;
    }
    return v*v;
  }
  a.forEach(poison);
})();

// Same, but for a double array.
(()=>{
  let a = [1, 2.5, 3,,,, 7];
  function poison(v, i) {
    if (i === 2) {
      [].__proto__[4] = 3;
    }
    return v*v;
  }
  a.forEach(poison);
})();
