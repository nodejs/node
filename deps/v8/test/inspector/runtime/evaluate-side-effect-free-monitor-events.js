// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PoC: UnsafeCommandLineAPIFns deny-list does not cover embedder-installed
// command-line-API names. Blink installs `monitorEvents` / `unmonitorEvents`
// via V8InspectorClient::installAdditionalCommandLineAPI; CommandLineAPIScope
// then exposes them on the global proxy with a kHasNoSideEffect getter, so
// they survive throwOnSideEffect:true (DevTools eager-eval) -- the exact
// threat the deny-list at v8-console.cc:1035-1042 is meant to prevent.
//
// Run with the patched inspector-test (see test-harness.patch), which makes
// the test client's installAdditionalCommandLineAPI add `monitorEvents` to
// the commandLineAPI object exactly as Blink does.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Embedder-installed command-line-API names bypass UnsafeCommandLineAPIFns');

async function probe(expression) {
  const {result} = await Protocol.Runtime.evaluate({
    expression,
    includeCommandLineAPI: true,
    throwOnSideEffect: true,
    replMode: true,
  });
  if (result.exceptionDetails) {
    InspectorTest.log(
        `[${expression}] -> THROWS: ` +
        result.exceptionDetails.exception.description.split('\n')[0]);
  } else {
    InspectorTest.log(
        `[${expression}] -> OK: type=${result.result.type}` +
        (result.result.description ? ` desc="${result.result.description}"` :
                                     ''));
  }
}

(async () => {
  // Control 1: a name in the V8 deny-list. Reading it under
  // throwOnSideEffect:true must throw "Possible side-effect".
  await probe('debug');

  // Control 2: a V8-side name explicitly carved out of the deny-list.
  await probe('$0');

  // BUG: embedder-installed names. These are iterated by the same
  // CommandLineAPIScope loop (v8-console.cc:1066) but, because they are not
  // in the hard-coded deny-list, get a kHasNoSideEffect getter. Under the
  // threat model documented at v8-console.cc:1035-1037, that lets a page
  // stash the function reference during eager-eval. In Blink the stashed
  // monitorEvents() then attaches a page-context listener to a same-process
  // cross-origin Window without a BindingSecurity check.
  await probe('monitorEvents');
  await probe('unmonitorEvents');

  // Show the function value is actually delivered (not just "no exception").
  await probe('typeof monitorEvents');

  InspectorTest.completeTest();
})();
