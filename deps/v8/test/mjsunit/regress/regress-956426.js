// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var b = { x: 0, y: 0, 0: '' };
var a = { x: 0, y: 100000000000, 0: '' };
Object.seal(b);
b.x = '';
