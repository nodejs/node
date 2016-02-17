// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

if (this["debug"]) debug.Debug.setListener(function() {});
var source = "var outer = 0; function test() {'use strict'; outer = 1; } test(); print('ok');";
function test_function() { eval(source); }
assertDoesNotThrow(test_function);
