// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that lastIndex of a global RegExp is overwritten as per
// ECMA-262 6.0 21.2.5.8 step 10.c.

var global = /./g;
global.lastIndex = { valueOf: function() { assertUnreachable(); } };
assertEquals("X", "x".replace(global, function(a) { return "X"; }));
assertEquals(0, global.lastIndex);
