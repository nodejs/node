// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts

var a = [1];
a.foo = "bla";
delete a.foo;
a[0] = 1.5;

var a2 = [];
a2.foo = "bla";
delete a2.foo;
