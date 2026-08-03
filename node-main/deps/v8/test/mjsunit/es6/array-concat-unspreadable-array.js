// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var array = [1, 2, 3];
assertEquals(array, [].concat(array));
assertEquals(array, array.concat([]));
array[Symbol.isConcatSpreadable] = false;
assertEquals([[1,2,3]], [].concat(array));
assertEquals([[1,2,3]], array.concat([]));
