// Flags: --max-old-space-size=64
'use strict';

// Regression test for https://github.com/nodejs/node/issues/51452
// When Promise.race() settles, V8 fires kPromiseResolveAfterResolved /
// kPromiseRejectAfterResolved for each "losing" promise. Before this fix,
// the C++ PromiseRejectCallback crossed into JS for these no-op events,
// accumulating references and causing OOM in tight async loops.
// With --max-old-space-size=64, this test would crash before completing
// if the leak is present.

const common = require('../common');

async function main() {
  for (let i = 0; i < 100_000; i++) {
    await Promise.race([
      Promise.resolve(1),
      Promise.resolve(2),
      Promise.resolve(3),
    ]);
  }
}

main().then(common.mustCall());
