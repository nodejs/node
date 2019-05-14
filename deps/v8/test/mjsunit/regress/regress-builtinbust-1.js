// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function nope() { return false; }
var a = [ 1, 2, 3 ];
Object.seal(a);
Object.isSealed = nope;

assertThrows(function() { a.pop(); }, TypeError);
assertThrows(function() { a.push(5); }, TypeError);
assertThrows(function() { a.shift(); }, TypeError);
assertThrows(function() { a.unshift(5); }, TypeError);
assertThrows(function() { a.splice(0, 1); }, TypeError);
