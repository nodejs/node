// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --young-generation-large-objects

function v2() {
    const v8 = Symbol || 9007199254740991;
    function v9(v10,v11,v12) {
    }
    const v16 = String();
    const v100 = String();//add
    const v106 = String();// add
    const v116 = String();// add
    const v17 = Int32Array();
    const v18 = Map();
    const v19 = [];
    const v20 = v18.values();
    function v21(v22,v23,v24,v25,v26) {
    }
    function v28(v29,v30,v31) {
        function v32(v33,v34,v35,v36) {
        }
        let v39 = 0;
        do {
            const v40 = v32();
            function v99() {
            }
        } while (v39 < 8);
    }
    const v41 = Promise();
}
const v46 = ["has",13.37,-9007199254740991,Reflect];
for (let v50 = 64; v50 <= 2000; v50++) {
    v46.push(v50,v2);
}
const v54 = RegExp(v46);
const v55 = v54.exec();

assertTrue(%HasElementsInALargeObjectSpace(v55));
