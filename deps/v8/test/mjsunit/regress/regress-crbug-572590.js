// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap

function g() { }
var f = g.bind();
f.__defineGetter__('length', g);
gc();
