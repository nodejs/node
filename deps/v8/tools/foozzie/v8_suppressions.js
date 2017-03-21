// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is loaded before each correctness test case and after v8_mock.js.
// You can temporarily change JS behavior here to silence known problems.
// Please refer to a bug in a comment and remove the suppression once the
// problem is fixed.

// Suppress http://crbug.com/662429
var __real_Math_pow = Math.pow
Math.pow = function(a, b){
  if (b < 0) {
    return 0.000017;
  } else {
    return __real_Math_pow(a, b);
  }
}
