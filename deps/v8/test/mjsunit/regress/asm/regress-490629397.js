// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

let leaked_global = null;

Error.prepareStackTrace = (err, stack) => {
  for (let i = 0; i < stack.length; i++) {
    const f = stack[i];
    let name = '';
    try {
      name = f.getFunctionName() || f.getMethodName();
    } catch (e) {
    }
    console.log('Frame', i, 'name:', name);
    try {
      let t = f.getThis();
      if (t && t.top_secret) {
        console.log('Found leaked global on frame', i);
        leaked_global = t;
      }
    } catch (e) {
    }
  }
  return 'custom stack';
};

// 1. Create a ShadowRealm and define a secret variable inside it.
const sr = new ShadowRealm();
sr.evaluate(`globalThis.top_secret = "Leaked from ShadowRealm!";`);

// 2. Create an asm.js module inside the ShadowRealm
// and return a function that instantiates it and calls a provided callback.
const getAsmExport = sr.evaluate(`
  () => {
    return (callback) => {
      function asmModule(stdlib, foreign, heap) {
        "use asm";
        var cb = foreign.cb;
        function call_cb() {
          cb();
        }
        return { call_cb: call_cb };
      }
      var instance = asmModule(globalThis, { cb: callback }, new ArrayBuffer(0x10000));
      instance.call_cb();
    };
  }
`);

// 3. Define a callback in the main realm that throws an error,
// which will trigger our custom prepareStackTrace.
function mainRealmThrower() {
  const e = new Error('Trigger prepareStackTrace');
  const dummy = e.stack;
}

// 4. Instantiate the asm.js module in the ShadowRealm and trigger the throw.
try {
  const instantiateAndCall = getAsmExport();
  instantiateAndCall(mainRealmThrower);
} catch (e) {
  console.log('Caught:', e.message);
}

// 5. Check if we successfully leaked the ShadowRealm's global object.
if (leaked_global) {
  console.log('SUCCESS! Leaked global this.');
  try {
    console.log('Secret value:', leaked_global.top_secret);
  } catch (e) {
  }
} else {
  console.log('FAILED to leak global this.');
}

assertNull(leaked_global);
