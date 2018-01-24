// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(o, k) { return o[k]; }

var a = [1,2];
a["-1"] = 42;

assertEquals(1, foo(a, 0));
assertEquals(2, foo(a, 1));
assertEquals(undefined, foo(a, 3));
assertEquals(42, foo(a, -1));
