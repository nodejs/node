// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function main() {
    for (let v3 = 0; v3 < 120; v3++) {
        const v6 = [Int16Array,1111];
        let v12 = 577623200;
        const v14 = [2];
        const v18 = [1.7976931348623157e+308,1.7976931348623157e+308,1.7976931348623157e+308,1.7976931348623157e+308];
        const v20 = [1111,Uint8Array];
        const v21 = [v20,v20,v18,v20,1111,1111,1111,-1111];
        const v23 = [11.11,11.11,1.7976931348623157e+308,11.11,11.11];
        const v26 = -Infinity;
        const v27 = [v23,v26,1111,v18,Date,1111,-9007199254740992,v21];
        const v31 = [v14];
        const v32 = [v31,v12,"object",v21,1111,6.0,v18,v27,Int8Array];
        const v33 = ["65555",v26,v32];
        const v34 = v33.toLocaleString();
        let v35 = "659874589";
        v35 = v34;
        const v37 = [11.11,11.11,1111];
        const v38 = [v6];
        const v39 = [v38,v37,v38];
        v37[10000] = v23;
        v12 = v35;
        const v54 = [parseInt,v39];
        const v56 = String.fromCharCode();
        const v61 = [v12,1111,-9007199254740991,1111];
        const v63 = [11.11,v54,JSON,v61,11.11,v56,v61];
        const v64 = JSON.stringify(v63);
        const v65 = RegExp(v64);
        const v66 = v65.exec(v64);
    }
}

main();
