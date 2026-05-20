// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function a(b, c) {
  if (c != b) 'xxxxxxxxxxxxxxxxx' + c
}

%PrepareFunctionForOptimization(a);

(function () {
  a("", ".")
})();

(function () {
  for (d = 2000; d; --d) {
    e = "123";
  }
  a(3, e.length)
})();
