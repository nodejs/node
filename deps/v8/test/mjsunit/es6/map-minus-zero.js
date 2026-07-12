// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var map = new Map();

var objectKey = {};
var stringKey = 'keykeykey';
var numberKey = 42.24;
var booleanKey = true;
var undefinedKey = undefined;
var nullKey = null;
var nanKey = NaN;
var zeroKey = 0;
var minusZeroKey = -0;

assertEquals(map.size, 0);

map.set(objectKey, 'aaa');
map.set(stringKey, 'bbb');
map.set(numberKey, 'ccc');
map.set(booleanKey, 'ddd');
map.set(undefinedKey, 'eee');
map.set(nullKey, 'fff');
map.set(nanKey, 'ggg');
map.set(zeroKey, 'hhh');

assertEquals(8, map.size);

assertEquals('aaa', map.get(objectKey));
assertEquals('bbb', map.get(stringKey));
assertEquals('ccc', map.get(numberKey));
assertEquals('ddd', map.get(booleanKey));
assertEquals('eee', map.get(undefinedKey));
assertEquals('fff', map.get(nullKey));
assertEquals('ggg', map.get(nanKey));
assertEquals('hhh', map.get(zeroKey));

assertEquals(undefined, map.get({}));
assertEquals('bbb', map.get('keykeykey'));
assertEquals('ccc', map.get(42.24));
assertEquals('ddd', map.get(true));
assertEquals('eee', map.get(undefined));
assertEquals('fff', map.get(null));
assertEquals('ggg', map.get(NaN));
assertEquals('hhh', map.get(0));
assertEquals('hhh', map.get(-0));
assertEquals('hhh', map.get(1 / Infinity));
assertEquals('hhh', map.get(-1 / Infinity));
