// Flags: --expose-gc

import '../common/index.mjs';
import { gcUntil } from '../common/gc.js';
import assert from 'node:assert/strict';
import { it } from 'node:test';

// The default abort reason must not keep the objects on the caller's stack
// alive. Refs: https://github.com/nodejs/node/issues/66192

class Job {
  controller = new AbortController();

  cancel() {
    this.controller.abort();
    return this.controller.signal;
  }

  cancelStatic() {
    return AbortSignal.abort();
  }
}

for (const method of ['cancel', 'cancelStatic']) {
  it(`does not retain the caller through the default reason (${method})`, async () => {
    let job = new Job();
    const jobRef = new WeakRef(job);
    const signal = job[method]();
    job = null;

    await gcUntil('job is collected', () => jobRef.deref() === undefined);
    assert.strictEqual(signal.aborted, true);
    assert.strictEqual(signal.reason.name, 'AbortError');
    assert.match(signal.reason.stack, new RegExp(`at Job\\.${method} `));
  });
}

it('does not throw when Error.prepareStackTrace throws', () => {
  const { prepareStackTrace } = Error;
  Error.prepareStackTrace = () => { throw new Error('boom'); };
  try {
    assert.strictEqual(new Job().cancel().reason.name, 'AbortError');
    assert.strictEqual(new Job().cancelStatic().reason.name, 'AbortError');
  } finally {
    Error.prepareStackTrace = prepareStackTrace;
  }
});
