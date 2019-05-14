// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(v5,v6) {
  const v16 = [1337,1337,-765470.5051836492];
  let v19 = 0;
  do {
    const v20 = v19 + 1;
    const v22 = Math.fround(v20);
    v19 = v22;
    const v23 = [v20, v22];
    function v24() { v20; v22; }
    const v33 = v16.indexOf(v19);
  } while (v19 < 6);
}

f();
Array.prototype.push(8);
%OptimizeFunctionOnNextCall(f);
f();
