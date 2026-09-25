// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var s = "function __f_9(func, testName) {" +
  "var __v_0 = function __f_10(__v_14, __v_14) {" +
  "  return __v_16;" +
  "}; " +
"}"
assertThrows(function() { eval(s); });
