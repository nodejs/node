// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags:

let a = [0, 1, 2, 3, 4];
a.pop();
a.pop();
a[2] = undefined;
a.pop();
delete a[0];
var p = Object.getPrototypeOf(0);
a[p] = -1n;
