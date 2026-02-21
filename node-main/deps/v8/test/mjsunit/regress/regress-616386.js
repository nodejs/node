// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy

assertEquals(0, ((y = (function(a2) { bbbb = a2 }), bbbb = eval('1')) => {y(0); return bbbb})())
assertEquals(0, (({y = (function(a2) { bbbb = a2 }), bbbb = eval('1')} = {}) => {y(0); return bbbb})())
assertEquals(0, (function (y = (function(a2) { bbbb = a2 }), bbbb = eval('1')) {y(0); return bbbb})())
assertEquals(0, (function ({y = (function(a2) { bbbb = a2 }), bbbb = eval('1')} = {}) {y(0); return bbbb})())
