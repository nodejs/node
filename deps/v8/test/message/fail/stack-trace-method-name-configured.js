// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check that the user configured method name ('SomeClass.someMethod') is used
// as-is in error.stack output without prepending the inferred type name ('A').
const A = function() {};
A.prototype.a = function() {
  throw new Error;
};
Object.defineProperty(A.prototype.a, 'name', {value: 'SomeClass.someMethod'});
(new A).a();
