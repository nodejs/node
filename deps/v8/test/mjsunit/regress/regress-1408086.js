// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const obj1 = {a:42};
const obj2 = {a:42};
function foo() {
    obj1.a = 13.37;
    return obj2;
}

class c1 extends foo {
    obj3 = 1;
}
new c1();
new c1();
