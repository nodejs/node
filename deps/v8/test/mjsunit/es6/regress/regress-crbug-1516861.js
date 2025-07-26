// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function F0() {
}
class C2 extends F0 {
    constructor() {
        for (let v7 = 0; v7 < 5; v7++) {
            return undefined;
        }
        super();
    }
}
assertThrows(() => new C2(), ReferenceError);
