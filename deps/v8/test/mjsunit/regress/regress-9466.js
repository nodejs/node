// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const o = [];
o.__proto__ = {};
o.constructor = function() {};
o.constructor[Symbol.species] = function f() {};
o.__proto__ = Array.prototype;
assertEquals(o.constructor[Symbol.species], o.concat([1,2,3]).constructor);
