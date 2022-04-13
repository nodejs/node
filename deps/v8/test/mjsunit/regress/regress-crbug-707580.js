// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var thrower = { [Symbol.toPrimitive] : function() { throw "I was called!" } };
var heap_number = 4.2;
var smi_number = 23;

assertThrows(() => heap_number.hasOwnProperty(thrower));
assertThrows(() => smi_number.hasOwnProperty(thrower));
