// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = [0,1,2,,,,7];
var proto = {}
a.__proto__ = proto;
var visits = 0;
Array.prototype.forEach.call(a, (v,i,o) => { ++visits; proto[4] = 4; });
assertEquals(5, visits);
