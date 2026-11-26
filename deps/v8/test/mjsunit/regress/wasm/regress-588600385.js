// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --strict-termination-checks

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const startImport = builder.addImport('m', 'onStart', kSig_v_v);
builder.addFunction('f', kSig_v_v).addBody([]);
builder.addStart(startImport);
const wireBytes = builder.toBuffer();

if (typeof Worker !== 'undefined') {
  // 1. Streaming Wasm instantiation queued from a microtask before termination
  // in a worker must not run its FinishCompilation foreground task after
  // MicrotasksScope::PerformCheckpoint terminates.
  {
    const w = new Worker(`
      let bytes;
      let cachedModule;
      let ready = false;
      onmessage = (e) => {
        if (!ready) {
          bytes = new Uint8Array(e.data);
          // Populate NativeModuleCache (holding a strong reference so GC cannot
          // evict it) so streaming hits prefix_cache_hit_ and posts a deferred
          // FinishCompilation foreground task.
          cachedModule = new WebAssembly.Module(bytes);
          ready = true;
          postMessage('ready');
          return;
        }
        const ia = new Int32Array(new SharedArrayBuffer(4));
        Atomics.waitAsync(ia, 0, 0).value.then(() => {
          let terminated = false;
          WebAssembly.instantiateStreaming(bytes, {
            m: {
              onStart() {
                if (terminated) postMessage('start_after_terminate');
              }
            }
          });
          Promise.resolve().then(() => {
            terminated = true;
            d8.terminateNow();
            // Only reached when termination is suppressed (e.g. foozzie).
            terminated = false;
            close();
          });
        });
        Atomics.notify(ia, 0);
      };
    `, {type: 'string'});
    w.postMessage(wireBytes);
    assertEquals('ready', w.getMessage());
    w.postMessage(0);
    assertEquals(undefined, w.getMessage());
  }

  // 2. General foreground tasks (e.g. setTimeout) must also not be pumped by
  // Worker::ProcessMessages after a microtask terminates execution.
  {
    const w = new Worker(`
      let ready = false;
      onmessage = () => {
        if (!ready) {
          ready = true;
          postMessage('ready');
          return;
        }
        const ia = new Int32Array(new SharedArrayBuffer(4));
        Atomics.waitAsync(ia, 0, 0).value.then(() => {
          let terminated = false;
          setTimeout(() => {
            if (terminated) postMessage('timeout_after_terminate');
          }, 0);
          terminated = true;
          d8.terminateNow();
          // Only reached when termination is suppressed (e.g. foozzie).
          terminated = false;
          close();
        });
        Atomics.notify(ia, 0);
      };
    `, {type: 'string'});
    w.postMessage(0);
    assertEquals('ready', w.getMessage());
    w.postMessage(0);
    assertEquals(undefined, w.getMessage());
  }

  // 3. A worker that defines onmessage and terminates during initial
  // Shell::FinishExecuting (ProcessMessages) must not subsequently enter
  // Worker::ProcessMessages and handle incoming messages.
  {
    const w = new Worker(`
      let terminated = false;
      onmessage = () => {
        if (terminated) postMessage('onmessage_after_terminate');
      };
      setTimeout(() => {
        terminated = true;
        postMessage('terminating');
        d8.terminateNow();
        // Only reached when termination is suppressed (e.g. foozzie).
        terminated = false;
        close();
      }, 0);
    `, {type: 'string'});
    assertEquals('terminating', w.getMessage());
    w.postMessage(0);
    assertEquals(undefined, w.getMessage());
  }
}
