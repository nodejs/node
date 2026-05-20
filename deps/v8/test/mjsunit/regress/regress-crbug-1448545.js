// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing

// --fuzzing is required to trigger the bug since it does a second compile
// --that'll check for identical bytecode.

// Make more lexical bindings that need hole checks due to uses in inner
// functions than a 64-bitmap can hold.
let v1 = 0;
let v2 = 0;
let v3 = 0;
let v4 = 0;
let v5 = 0;
let v6 = 0;
let v7 = 0;
let v8 = 0;
let v9 = 0;
let v10 = 0;
let v11 = 0;
let v12 = 0;
let v13 = 0;
let v14 = 0;
let v15 = 0;
let v16 = 0;
let v17 = 0;
let v18 = 0;
let v19 = 0;
let v20 = 0;
let v21 = 0;
let v22 = 0;
let v23 = 0;
let v24 = 0;
let v25 = 0;
let v26 = 0;
let v27 = 0;
let v28 = 0;
let v29 = 0;
let v30 = 0;
let v31 = 0;
let v32 = 0;
let v33 = 0;
let v34 = 0;
let v35 = 0;
let v36 = 0;
let v37 = 0;
let v38 = 0;
let v39 = 0;
let v40 = 0;
let v41 = 0;
let v42 = 0;
let v43 = 0;
let v44 = 0;
let v45 = 0;
let v46 = 0;
let v47 = 0;
let v48 = 0;
let v49 = 0;
let v50 = 0;
let v51 = 0;
let v52 = 0;
let v53 = 0;
let v54 = 0;
let v55 = 0;
let v56 = 0;
let v57 = 0;
let v58 = 0;
let v59 = 0;
let v60 = 0;
let v61 = 0;
let v62 = 0;
let v63 = 0;
let v64 = 0;

function someUses() {
  v1 = 0;
  v2 = 0;
  v3 = 0;
  v4 = 0;
  v5 = 0;
  v6 = 0;
  v7 = 0;
  v8 = 0;
  v9 = 0;
  v10 = 0;
  v11 = 0;
  v12 = 0;
  v13 = 0;
  v14 = 0;
  v15 = 0;
  v16 = 0;
  v17 = 0;
  v18 = 0;
  v19 = 0;
  v20 = 0;
  v21 = 0;
  v22 = 0;
  v23 = 0;
  v24 = 0;
  v25 = 0;
  v26 = 0;
  v27 = 0;
  v28 = 0;
  v29 = 0;
  v30 = 0;
  v31 = 0;
  v32 = 0;
  v33 = 0;
  v34 = 0;
  v35 = 0;
  v36 = 0;
  v37 = 0;
  v38 = 0;
  v39 = 0;
  v40 = 0;
  v41 = 0;
  v42 = 0;
  v43 = 0;
  v44 = 0;
  v45 = 0;
  v46 = 0;
  v47 = 0;
  v48 = 0;
  v49 = 0;
  v50 = 0;
  v51 = 0;
  v52 = 0;
  v53 = 0;
  v54 = 0;
  v55 = 0;
  v56 = 0;
  v57 = 0;
  v58 = 0;
  v59 = 0;
  v60 = 0;
  v61 = 0;
  v62 = 0;
  v63 = 0;
  v64 = 0;
}

// Make another lexical binding that needs hole checks in the same scope with
// some uses that can be elided. Both the first and second compiles should be
// able to elide the subsequent use.
try {
  x = 42;
  x = 42;
} catch (e) {
}

let x = 0;
