// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var num = new Number(true)
// Number wrapper objects are not concat-spreadable by default
assertEquals([num], [].concat(num));

// Number wrapper objects may be individually concat-spreadable
num[Symbol.isConcatSpreadable] = true;
num.length = 3;
num[0] = 1, num[1] = 2, num[2] = 3;
assertEquals([1, 2, 3], [].concat(num));

Number.prototype[Symbol.isConcatSpreadable] = true;
// Number wrapper objects may be concat-spreadable
assertEquals([], [].concat(new Number(123)));
Number.prototype[0] = 1;
Number.prototype[1] = 2;
Number.prototype[2] = 3;
Number.prototype.length = 3;
assertEquals([1,2,3], [].concat(new Number(123)));

// Number values are never concat-spreadable
assertEquals([true], [].concat(true));
