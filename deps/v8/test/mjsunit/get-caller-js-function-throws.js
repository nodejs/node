// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noalways-opt --nostress-opt

// Ensure that "real" js functions that call GetCallerJSFunction get an
// exception, since they are not stubs.
(function() {
  var a = function() {
    return %_GetCallerJSFunction();
  }
  assertThrows(a);
}());
