// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check that the dynamic type name ('A') is not prepended to the inferred
// method name ('C.a') in error.stack output.
const A = function() {};
const C = A;
C.prototype.a = function() {
  throw new Error;
};
(new A).a();
