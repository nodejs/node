// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks --strict-termination-checks --js-defer-import-eval

function runInWorker(body) {
  if (typeof Worker === 'undefined') return;
  const worker = new Worker(`
    globalThis.arm = false;
    globalThis.count = 0;
    globalThis.terminated = false;
    async_hooks.createHook({
      promiseResolve() {
        if (globalThis.arm ||
            (globalThis.count > 0 && --globalThis.count === 0)) {
          globalThis.arm = false;
          globalThis.terminated = true;
          d8.terminateNow();
          // Reached only when termination is suppressed (differential fuzzing).
          globalThis.terminated = false;
        }
      }
    }).enable();
    ${body}
  `, {type: 'string'});
  assertEquals(undefined, worker.getMessage());
}

// 1. JSPromise::Resolve primitive branch via Runtime_ResolvePromise.
runInWorker(`
  globalThis.arm = true;
  try {
    new Promise(resolve => resolve(42));
  } catch {}
  if (globalThis.terminated) postMessage('escaped');
`);

// 2. JSPromise::Resolve self-resolution branch.
runInWorker(`
  let resolve_self;
  const p = new Promise(resolve => { resolve_self = resolve; });
  p.catch(() => {});
  globalThis.arm = true;
  try {
    resolve_self(p);
  } catch {}
  if (globalThis.terminated) postMessage('escaped');
`);

// 3. JSPromise::Resolve JSReceiver branch.
runInWorker(`
  globalThis.arm = true;
  try {
    new Promise(resolve => resolve({}));
  } catch {}
  if (globalThis.terminated) postMessage('escaped');
`);

// 4. AsyncDisposableStack.prototype.disposeAsync on an already-disposed stack.
runInWorker(`
  const stack = new AsyncDisposableStack();
  stack.disposeAsync();
  globalThis.arm = true;
  try {
    stack.disposeAsync();
  } catch {}
  if (globalThis.terminated) postMessage('escaped');
`);

// 5. AsyncDisposeFromSyncDispose resolving its inner promise.
runInWorker(`
  const stack = new AsyncDisposableStack();
  stack.use({
    [Symbol.dispose]() {
      globalThis.arm = true;
    }
  });
  try {
    stack.disposeAsync();
  } catch {}
  if (globalThis.terminated) postMessage('escaped');
`);

// 6. JSAsyncDisposableStack::NextDisposeAsyncIteration resolving outer promise.
runInWorker(`
  const stack = new AsyncDisposableStack();
  stack.use({
    [Symbol.dispose]() {
      globalThis.count = 2;
    }
  });
  stack.disposeAsync();
`);

// 7. SourceTextModule::Evaluate resolving capability on synchronous evaluation.
runInWorker(`
  const sync_leaf =
      'data:text/javascript,globalThis.arm = true; export var x = 1;';
  import(
      \`data:text/javascript,import defer * as ns from "\${sync_leaf}"; \` +
      'try { ns.x; } catch {} ' +
      'if (globalThis.terminated) postMessage("escaped");');
`);

// 8. SourceTextModule::AsyncModuleExecutionFulfilled ->
//    TransitionAsyncToEvaluated resolving top_level_capability on a synchronous
//    ancestor.
runInWorker(`
  const async_leaf =
      'data:text/javascript,await new Promise(r => { globalThis.resume = r; });';
  const sync_parent =
      \`data:text/javascript,import "\${async_leaf}"; \` +
      'globalThis.arm = true; export var y = 2;';
  import(sync_parent);
  setTimeout(() => {
    globalThis.resume();
  }, 0);
`);
