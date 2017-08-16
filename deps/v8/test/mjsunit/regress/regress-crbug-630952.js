// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt
try {
function __f_4(sign_bit,
                          mantissa_29_bits) {
}
__f_4.prototype.returnSpecial = function() {
                     this.mantissa_29_bits * mantissa_29_shift;
}
__f_4.prototype.toSingle = function() {
  if (-65535) return this.toSingleSubnormal();
}
__f_4.prototype.toSingleSubnormal = function() {
  if (__v_15) {
      var __v_7 = this.mantissa_29_bits == -1 &&
                 (__v_13 & __v_10 ) == 0;
    }
  __v_8 >>= __v_7;
}
__v_14 = new __f_4();
__v_14.toSingle();
} catch(e) {}
