// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(a) { return arguments; }
var a = f(1,2,3);
delete a[1];
Array.prototype.sort.apply(a);
a[10000000] = 4;
Array.prototype.sort.apply(a);
