// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = { x: 0 };
delete o.x;
function store(o, p, v) { o[p] = v; }
store(o, "x", 1);
store(o, "x", 1);
store(o, "0", 1);
