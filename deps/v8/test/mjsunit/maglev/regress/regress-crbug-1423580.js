// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function f_StoreTaggedFieldWithWriteBarrier() {
  try {
    function f4() {
    }
    "k"["k"]();
  } catch(e8) {
    e8.c = f_StoreTaggedFieldWithWriteBarrier;
    for (let v11 = 1261610744; v11 < 1261610744; v11 = v11 + 3) {
      // This code is unreachable. However, due to feedback sharing, this store
      // will have an initialized feedback slot: it shares its slot with
      // `e8.c = f0` despite the 2 having different receivers, because both receivers
      // have index 2: `e8` in the context, and `v11` in the local.
      v11.c = e8;
    }
    const t11 = 3;
  }
}

%PrepareFunctionForOptimization(f_StoreTaggedFieldWithWriteBarrier);
f_StoreTaggedFieldWithWriteBarrier();
%OptimizeMaglevOnNextCall(f_StoreTaggedFieldWithWriteBarrier);
f_StoreTaggedFieldWithWriteBarrier();


function f_CheckedStoreSmiField(obj) {
  obj.c = 42;
  for (let v11 = 3.5; v11 < 3.5; v11 = v11 + 3) {
    // This code is unreachable. However, due to feedback sharing, this store
    // will have an initialized feedback slot: it shares its slot with
    // `obj.c = f0` despite the 2 having different receivers, because they both
    // have index 0: `obj` in the parameters, and `v11` in the locals.
    v11.c = 4;
  }
}

%PrepareFunctionForOptimization(f_CheckedStoreSmiField);
f_CheckedStoreSmiField({c:42});
%OptimizeMaglevOnNextCall(f_CheckedStoreSmiField);
f_CheckedStoreSmiField({c:42});
