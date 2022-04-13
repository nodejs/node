// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = new Array();
a.prototype = a;

function f() {
 a.length = 0x2000001;
 a.push();
}

({}).__proto__ = a;

f()
f()

a.length = 1;
a.fill(-255);

%HeapObjectVerify(a);
