// Flags: --expose-gc
'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const { gcUntil } = require('../common/gc');

async function test() {
  for (const sandbox of [{}, vm.constants.DONT_CONTEXTIFY]) {
    let queue = vm.createMicrotaskQueue();
    const ref = new WeakRef(queue);
    const context = vm.createContext(sandbox, {
      microtaskMode: { type: 'manual', queue },
    });
    queue = null;
    await gcUntil('microtask queue handle', () => ref.deref() === undefined);

    // The V8 context still needs its queue after the handle has been collected.
    // Enqueuing a reaction would access freed memory without context ownership.
    assert.strictEqual(vm.runInContext(`
      globalThis.ran = false;
      Promise.resolve().then(() => { ran = true; });
      ran;
    `, context), false);
    global.gc();
    assert.strictEqual(vm.runInContext('ran', context), false);
  }
}

test().then(common.mustCall());
