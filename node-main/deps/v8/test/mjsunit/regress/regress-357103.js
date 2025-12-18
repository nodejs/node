// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

%SetAllocationTimeout(1, 1);

var key = "Huckleberry Finn" + "Tom Sawyer";
var o = {};
function f() { o[key] = "Adventures"; }

f();
f();
