// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (let i = 0; i < 4; ++i) {
    var obj1 = {
        get [obj1]() {},
        ...obj2,
    };
    var obj2 = { [obj1]: 0 };
    print(obj2);
}
