// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100 --sim-stack-size=100

function f() {
}

var large_array = Array(150 * 1024);
assertThrows('new f(... large_array)', RangeError);
