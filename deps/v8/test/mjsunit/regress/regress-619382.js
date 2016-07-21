// Copyright 2016 the V8 project authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.
//
// // Flags: --expose-gc --always-opt

(function __f_9() {
})();
function __f_16(ctor_desc) {
  var __v_22 = 5;
  var __v_25 = [];
  gc(); gc(); gc();
  for (var __v_18 = 0; __v_18 < __v_22; __v_18++) {
    __v_25[__v_18] = ctor_desc.ctor.apply();
  }
}
var __v_28 = [
  {
    ctor: function(__v_27) { return {a: __v_27}; },
    args: function() { return [1.5 + __v_18]; }  },
  {
    ctor: function(__v_27) { var __v_21 = []; __v_21[1] = __v_27; __v_21[200000] = __v_27; return __v_21; },
    args: function() { return [1.5 + __v_18]; }  },
  {
    ctor: function() {
    }  }
];
var __v_26 = [
  {
  }];
  __v_26.forEach(function(__v_16) {
    __v_28.forEach(function(ctor) {
      __f_16(ctor);
    });
  });
