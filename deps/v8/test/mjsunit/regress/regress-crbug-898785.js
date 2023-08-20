// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = [0, 1];
var o = { [Symbol.toPrimitive]() { a.length = 1; return 2; } };

a.push(2);
a.lastIndexOf(5, o);
