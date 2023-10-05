// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C1 {
    constructor(a3) {
        class C6 extends C1 {
            [this] = undefined;
            static 1 = a3;
            static {
               super.p();
            }
        }
    }
}
assertThrows(() => new C1(), TypeError);
