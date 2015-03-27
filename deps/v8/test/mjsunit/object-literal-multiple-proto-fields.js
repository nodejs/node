// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var p1 = {};
var p2 = {};
var p3 = {};
var x = 0;
var y = 1;
var z = 2;
var o = 3;
assertThrows(
    'o = {' +
    '  __proto__: (x++, p1),' +
    '  __proto__: (y++, p2),' +
    '  __proto__: (z++, p3)' +
    '};', SyntaxError);
assertEquals(0, x);
assertEquals(1, y);
assertEquals(2, z);
assertEquals(3, o);
