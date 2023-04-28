// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --track-field-types

function f1(a) { return {x:a, v:''}; }
function f2(a) { return {x:{v:a}, v:''}; }
function f3(a) { return {x:[], v:{v:''}}; }
f3([0]);
a = f1(1);
a.__defineGetter__('v', function() { gc(); return f2(this); });
a.v;
f3(1);
