// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var str1 = new String("yuck\uD83D\uDCA9")
// String wrapper objects are not concat-spreadable by default
assertEquals([str1], [].concat(str1));

// String wrapper objects may be individually concat-spreadable
str1[Symbol.isConcatSpreadable] = true;
assertEquals(["y", "u", "c", "k", "\uD83D", "\uDCA9"],
             [].concat(str1));

String.prototype[Symbol.isConcatSpreadable] = true;
// String wrapper objects may be concat-spreadable
assertEquals(["y", "u", "c", "k", "\uD83D", "\uDCA9"],
             [].concat(new String("yuck\uD83D\uDCA9")));

// String values are never concat-spreadable
assertEquals(["yuck\uD83D\uDCA9"], [].concat("yuck\uD83D\uDCA9"));
