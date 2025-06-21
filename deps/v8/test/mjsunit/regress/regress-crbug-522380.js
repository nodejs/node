// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = this;
global.__defineSetter__('x', function(v) { x = v; });
assertThrows("global.x = 0", RangeError);
