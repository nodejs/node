// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_i_v)
    .addBody([kExprI32Const, 42])
    .exportAs('f');
const buffer = builder.toBuffer();

const realm = Realm.create();

// Start async compilation in the other realm.
// We want to trigger context destruction (Realm.detach) while the compilation
// is in its final steps.
let promise = Realm.eval(realm, `
  WebAssembly.compile(new Uint8Array(${JSON.stringify(Array.from(buffer))}))
`);

// Detach (dispose) the realm. This should trigger DeleteCompileJobsOnContext.
// If FinishCompile was already scheduled on the foreground task queue,
// it might run after or during this destruction.
Realm.dispose(realm);

// We can also try to trigger it more specifically if we can.
// But Realm.detach is the most common way this happens in WebView (navigation).
// Give it some time to process tasks.
setTimeout(() => {
  // If we didn't crash, we're good.
  print("Success (no crash)");
}, 100);
