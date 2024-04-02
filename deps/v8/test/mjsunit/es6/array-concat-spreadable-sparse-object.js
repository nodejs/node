// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var obj = { length: 5 };
obj[Symbol.isConcatSpreadable] = true;
assertEquals([void 0, void 0, void 0, void 0, void 0], [].concat(obj));

obj.length = 4000;
assertEquals(new Array(4000), [].concat(obj));
