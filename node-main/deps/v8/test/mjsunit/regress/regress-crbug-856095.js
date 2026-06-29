// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(a, b, c) { }
function a() {
    let o1;
    o1 = new Array();
    f(...o1);
    o1[1000] = Infinity;
}

a();
a();
