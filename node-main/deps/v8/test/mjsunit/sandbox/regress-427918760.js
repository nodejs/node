// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

let sbx_mem = new DataView(new Sandbox.MemoryView(0, 0x500000000));

function corruptInBackground(address) {
    function workerTemplate(address) {
        let sbx_mem = new DataView(new Sandbox.MemoryView(0, 0x500000000));
        let switch_val = false;
        while(true) {
            switch_val = !switch_val;
            if (switch_val) {
                // '4'
                sbx_mem.setUint8(address, 0x34, true);
            } else {
                // ' '
                sbx_mem.setUint8(address, 0x20, true);
            }
        }
    }
    const workerCode = new Function(
        `(${workerTemplate})(${address})`);
    return new Worker(workerCode, { type: 'function' });
}

function GenerateSwitch(begin, end, handle_case) {
    var str = "function asmModule() {\
      \"use asm\";\
      function main(x) {\
        x = x|0;\
        switch(x|0) {";
    for (var i = begin; i <= end; i++) {
        str = str.concat("case ", i.toString(), ": ", handle_case(i));
    }
    str = str.concat("default: return -1;\
        }\
        return -2;\
      }\
      return {main: main}; } ");

    gc();
    gc();

    for (let ctr = 0; ctr < 10; ++ctr) {
        let new_str = str;
        let suffix = new Array(128);
        for (let i = 0; i < 128; i++) {
            suffix[i] = (Math.random() < 0.5) ? ' ' : '\t';
        }
        new_str += suffix.join('');

        var decl = eval('(' + new_str + ')');

        // wait until heap layout stabilizes
        if (ctr == 2) {
            done = true;
            let decl_addr = Sandbox.getAddressOf(decl);
            print("decl_addr: 0x" + decl_addr.toString(16));

            let str_addr = decl_addr;
            // find address of 'case 855:'
            while(true) {
                let v1 = sbx_mem.getUint32(str_addr);
                let v2 = sbx_mem.getUint32(str_addr + 4);
                if (v1 == 0x20383535 && v2 == 0x3a207265) {
                    print("found: 0x" + str_addr.toString(16));
                    break;
                }
                str_addr -= 1;
            }
            //                      case 855:
            // str_addr points now here ^
            corruptInBackground(str_addr);
        } else {
            var wasm1 = decl();
            for (var i = 0; i <= 1024; i = i + 3) {
                wasm1.main(i);
            }
        }

        gc();
        gc();
    }
}
var handle_case = function (k) {
    return "return ".concat(k, ";");
}

wasm = GenerateSwitch(0, 1024, handle_case);
