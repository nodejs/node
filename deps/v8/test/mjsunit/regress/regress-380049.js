// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a,b,c) { return arguments; }
var f = foo(false, null, 40);
assertThrows(function() { %ObjectFreeze(f); });
