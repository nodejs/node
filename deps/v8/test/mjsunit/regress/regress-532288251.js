// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --stress-runs=2 --no-wait-for-background-tasks --omit-quit --disable-in-process-stack-traces

// Stack-use-after-return in d8's InspectorClient.
//
// d8's InspectorClient is stack-allocated inside Shell::RunMainIsolate() and
// stashes a raw `this` pointer in the v8::Context's embedder data. It also
// installs a global `send()` whose native callback
// (InspectorClient::SendInspectorMessage) reads that embedder-data pointer
// and dereferences `inspector_client->session_`.
//
// With --stress-runs=2 the script is executed twice in two separate
// RunMainIsolate() stack frames, each with its own stack InspectorClient.
// With --no-wait-for-background-tasks, run #1 does *not* wait for the pending
// async WebAssembly compilation before RunMainIsolate() returns and destroys
// its InspectorClient. The compilation-finished foreground task
// (D8WasmAsyncResolvePromiseTask) is later pumped by run #2's
// FinishExecuting(); it resolves run #1's promise, whose reaction job runs in
// run #1's context and calls run #1's `send`, which dereferences the dead
// stack InspectorClient => stack-use-after-return.

// Minimal valid WebAssembly module: magic + version only.
const kMinimalWasm = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // \0asm
  0x01, 0x00, 0x00, 0x00,  // version 1
]);

// Capture this run's `send` so the delayed reaction still uses run #1's
// binding after run #2 replaces the global.
const capturedSend = send;

// Nudge ASAN's fake-stack round-robin allocator so that, at crash time, the
// dead RunMainIsolate slot is in the poisoned "after-return" state instead of
// being coincidentally recycled by a live frame in the same size class.
// (Without this the same root cause manifests as a null-pointer SEGV in
// SendInspectorMessage rather than a clean stack-use-after-return report.)
for (let i = 0; i < 3; i++) JSON.parse('{"a":[1,2,3]}');

// Kick off async wasm compilation. Its completion is delivered via a
// foreground D8WasmAsyncResolvePromiseTask which, thanks to
// --no-wait-for-background-tasks, can land after this run's RunMainIsolate()
// stack frame has already returned.
if (typeof WebAssembly !== 'undefined') {
  WebAssembly.compile(kMinimalWasm).then(
    (mod) => {
      print("Wasm loaded in run: " + (mod instanceof WebAssembly.Module));
      // Triggers InspectorClient::SendInspectorMessage -> GetSession(context),
      // which reads the stale InspectorClient* from run #1's context embedder
      // data and does `inspector_client->session_.get()` on dead stack memory.
      capturedSend('{"id":1,"method":"Runtime.enable"}');
    },
    (e) => {
      print("compile rejected: " + e);
      capturedSend('{"id":1,"method":"Runtime.enable"}');
    }
  );
}
