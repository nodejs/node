// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
// If @@isConcatSpreadable is not used, the value of IsArray(O)
// is used to determine the spreadable property.
class A extends Array {}
var obj = [].concat(new A(1, 2, 3), new A(4, 5, 6), new A(7, 8, 9));
assertEquals(9, obj.length);
for (var i = 0; i < obj.length; ++i) {
  assertEquals(i + 1, obj[i]);
}

// TODO(caitp): when concat is called on instances of classes which extend
// Array, they should:
//
// - return an instance of the class, rather than an Array instance (if from
//   same Realm)
// - always treat such classes as concat-spreadable
