// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

// Test that CallSite#getFunction and CallSite#getThis don't allow cross
// ShadowRealm boundary access.
(function testWasmInstance() {
  if (typeof WebAssembly === 'undefined') return;  // Skip on jitless.

  globalThis[Symbol.toStringTag] = "MainGlobal";
  Error.prepareStackTrace = function(err, frames) {
    let a = [];
    for (let i = 0; i < frames.length; i++) {
      try {
        frames[i].getFunction();
        a.push("functionName: " + frames[i].getFunctionName());
      } catch (e) {
        a.push(frames[i].getFunctionName() + " threw");
      }
      try {
        let t = frames[i].getThis();
        if (t === undefined) {
          a.push("this: undefined");
        } else {
          a.push("this: " + t);
        }
      } catch (e) {
        a.push("getThis threw");
      }
    }
    return a.join(' ');
  };

  const sr = new ShadowRealm();

  // Inside the SR, build a *non-asm.js* WebAssembly module that imports a
  // callback and exports `run` which calls it:
  //    (module (import "js" "cb" (func $cb)) (func (export "run") (call $cb)))
  // Return a wrapped callable that instantiates the module and calls run().
  const callIntoWasm = sr.evaluate(`
    globalThis[Symbol.toStringTag] = "RealmGlobal";
    const bytes = new Uint8Array([
      0x00, 0x61, 0x73, 0x6d,  // wasm magic
      0x01, 0x00, 0x00, 0x00,  // wasm version

      0x01,                    // section kind: Type
      0x04,                    // section length 4
      0x01, 0x60,              // types count 1:  kind: func
      0x00,                    // param count 0
      0x00,                    // return count 0

      0x02,                    // section kind: Import
      0x09,                    // section length 9
      0x01, 0x02,              // imports count 1: module name length:  2
      0x6a, 0x73,              // module name: js
      0x02,                    // field name length:  2
      0x63, 0x62,              // field name: cb
      0x00, 0x00,              // kind or compact flag: function
                               // import #0

      0x03,                    // section kind: Function
      0x02,                    // section length 2
      0x01, 0x00,              // functions count 1: 1 $run

      0x07,                    // section kind: Export
      0x07,                    // section length 7
      0x01,                    // exports count 1: export # 0
      0x03,                    // field name length:  3
      0x72, 0x75, 0x6e,        // field name: run
      0x00, 0x01,              // kind: function index:  1

      0x0a,                    // section kind: Code
      0x06,                    // section length 6
      0x01,                    // functions count 1
                               // function #1 $run
      0x04,                    // body size 4
      0x00,                    // 0 entries in locals list
      0x10, 0x00,              // call $js.cb
      0x0b,                    // end
    ]);
    const mod = new WebAssembly.Module(bytes);
    (function (cb) {
      const inst = new WebAssembly.Instance(mod, { js: { cb: cb } });
      return inst.exports.run();
    })
  `);

  function mainRealmThrower() {
    let e = new Error();
    // Trigger stack rendering while still inside main realm, otherwise
    // the stack trace will be rendered by shadow realm when crossing the
    // boundary and it wouldn't have access to main realm's call sites.
    e.stack;
    throw e;
  }

  let captured;
  try {
    callIntoWasm(mainRealmThrower);
  } catch (e) {
    captured = e.stack;
  }

  // There must be no Wasm instances in the stack trace since it belongs to
  // the shadow realm.
  assertEquals("functionName: mainRealmThrower this: [object MainGlobal] " +
               "functionName: null getThis threw " +
               "eval threw getThis threw " +
               "functionName: testWasmInstance this: [object MainGlobal] " +
               "functionName: null this: [object MainGlobal]", captured);
})();
