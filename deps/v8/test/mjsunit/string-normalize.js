// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals('', ''.normalize());
assertTrue(delete Array.prototype.indexOf);
assertEquals('', ''.normalize());

assertThrows(function() { ''.normalize('invalid'); }, RangeError);
assertTrue(delete Array.prototype.join);
assertThrows(function() { ''.normalize('invalid'); }, RangeError);

// All of these toString to an invalid form argument.
assertThrows(function() { ''.normalize(null) }, RangeError);
assertThrows(function() { ''.normalize(true) }, RangeError);
assertThrows(function() { ''.normalize(false) }, RangeError);
assertThrows(function() { ''.normalize(42) }, RangeError);
assertThrows(function() { ''.normalize({}) }, RangeError);
assertThrows(function() { ''.normalize([]) }, RangeError);
