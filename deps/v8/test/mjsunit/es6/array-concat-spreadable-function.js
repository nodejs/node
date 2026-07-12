// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var fn = function(a, b, c) {}
// Functions are not concat-spreadable by default
assertEquals([fn], [].concat(fn));

// Functions may be individually concat-spreadable
fn[Symbol.isConcatSpreadable] = true;
fn[0] = 1, fn[1] = 2, fn[2] = 3;
assertEquals([1, 2, 3], [].concat(fn));

Function.prototype[Symbol.isConcatSpreadable] = true;
// Functions may be concat-spreadable
assertEquals(new Array(3), [].concat(function(a,b,c) {}));
Function.prototype[0] = 1;
Function.prototype[1] = 2;
Function.prototype[2] = 3;
assertEquals([1,2,3], [].concat(function(a, b, c) {}));
