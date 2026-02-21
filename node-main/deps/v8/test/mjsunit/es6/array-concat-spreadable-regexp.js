// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var re = /abc/;
// RegExps are not concat-spreadable by default
assertEquals([re], [].concat(re));

// RegExps may be individually concat-spreadable
re[Symbol.isConcatSpreadable] = true;
re[0] = 1, re[1] = 2, re[2] = 3, re.length = 3;
assertEquals([1, 2, 3], [].concat(re));

// RegExps may be concat-spreadable
RegExp.prototype[Symbol.isConcatSpreadable] = true;
RegExp.prototype.length = 3;

assertEquals(new Array(3), [].concat(/abc/));
RegExp.prototype[0] = 1;
RegExp.prototype[1] = 2;
RegExp.prototype[2] = 3;
assertEquals([1,2,3], [].concat(/abc/));
