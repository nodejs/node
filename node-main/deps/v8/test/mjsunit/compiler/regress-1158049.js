// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const v2 = Symbol.unscopables;

function v6(v7,v8,v9,v10) {
  try {
    let v11 = eval && v2;
    const v12 = v11++;
  } catch(v13) {}
}

for (let v17 = 1; v17 < 10000; v17++) {
  const v18 = v6();
}
