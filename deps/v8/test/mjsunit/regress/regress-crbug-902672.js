// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = this;
var b = {};
a.length = 4294967296; // 2 ^ 32 (max array length + 1)
assertThrows(() => Array.prototype.join.call(a,b), TypeError);
