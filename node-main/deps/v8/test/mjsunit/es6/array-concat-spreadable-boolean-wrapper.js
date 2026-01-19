// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var bool = new Boolean(true)
// Boolean wrapper objects are not concat-spreadable by default
assertEquals([bool], [].concat(bool));

// Boolean wrapper objects may be individually concat-spreadable
bool[Symbol.isConcatSpreadable] = true;
bool.length = 3;
bool[0] = 1, bool[1] = 2, bool[2] = 3;
assertEquals([1, 2, 3], [].concat(bool));

Boolean.prototype[Symbol.isConcatSpreadable] = true;
// Boolean wrapper objects may be concat-spreadable
assertEquals([], [].concat(new Boolean(true)));
Boolean.prototype[0] = 1;
Boolean.prototype[1] = 2;
Boolean.prototype[2] = 3;
Boolean.prototype.length = 3;
assertEquals([1,2,3], [].concat(new Boolean(true)));

// Boolean values are never concat-spreadable
assertEquals([true], [].concat(true));
