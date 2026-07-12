// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function v1(v4) {
    let v6 = 0;
    const v8 = v6--;
    for (let v11 = v6; v11 < 6; v11++) {
        v4 = v6;
    }
    try {
1(1,..."string",1024);
    } catch(v18) {
    }
}

for (let v19 = 0; v19 < 100; v19++) {
    const v20 = v1();
}
