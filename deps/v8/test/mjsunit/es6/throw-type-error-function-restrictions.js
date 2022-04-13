// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var throwTypeErrorFunction =
    Object.getOwnPropertyDescriptor(Function.prototype, 'arguments').get;

var nameDesc =
    Object.getOwnPropertyDescriptor(throwTypeErrorFunction, 'name');
assertFalse(nameDesc.configurable, 'configurable');
assertFalse(nameDesc.enumerable, 'enumerable');
assertFalse(nameDesc.writable, 'writable');
assertThrows(function() {
  'use strict';
  throwTypeErrorFunction.name = 'foo';
}, TypeError);

var lengthDesc =
    Object.getOwnPropertyDescriptor(throwTypeErrorFunction, 'length');
assertFalse(lengthDesc.configurable, 'configurable');
assertFalse(lengthDesc.enumerable, 'enumerable');
assertFalse(lengthDesc.writable, 'writable');
assertThrows(function() {
  'use strict';
  throwTypeErrorFunction.length = 42;
}, TypeError);

assertTrue(Object.isFrozen(throwTypeErrorFunction));
