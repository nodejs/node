// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var set = new Set();

var objectKey = {};
var stringKey = 'keykeykey';
var numberKey = 42.24;
var booleanKey = true;
var undefinedKey = undefined;
var nullKey = null;
var nanKey = NaN;
var zeroKey = 0;
var minusZeroKey = -0;

assertEquals(set.size, 0);

set.add(objectKey);
set.add(stringKey);
set.add(numberKey);
set.add(booleanKey);
set.add(undefinedKey);
set.add(nullKey);
set.add(nanKey);
set.add(zeroKey);

assertEquals(8, set.size);

assertTrue(set.has(objectKey));
assertTrue(set.has(stringKey));
assertTrue(set.has(numberKey));
assertTrue(set.has(booleanKey));
assertTrue(set.has(undefinedKey));
assertTrue(set.has(nullKey));
assertTrue(set.has(nanKey));
assertTrue(set.has(zeroKey));

assertFalse(set.has({}));
assertTrue(set.has('keykeykey'));
assertTrue(set.has(42.24));
assertTrue(set.has(true));
assertTrue(set.has(undefined));
assertTrue(set.has(null));
assertTrue(set.has(NaN));
assertTrue(set.has(0));
assertTrue(set.has(-0));
assertTrue(set.has(1 / Infinity));
assertTrue(set.has(-1 / Infinity));
