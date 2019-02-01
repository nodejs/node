'use strict';

const common = require('../common');

const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  const w = new Worker(__filename, {
    stdin: true,
    stdout: true,
    stderr: true
  });

  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);

    // `postMessage` should not throw after termination
    // (this mimics the browser behavior).
    assert.doesNotThrow(() => w.postMessage('foobar'));
    assert.doesNotThrow(() => w.ref());
    assert.doesNotThrow(() => w.unref());

    // Although not browser specific, probably wise to
    // make sure the stream getters don't throw either.
    assert.doesNotThrow(() => w.stdin);
    assert.doesNotThrow(() => w.stdout);
    assert.doesNotThrow(() => w.stderr);

    // Sanity check.
    assert.strictEqual(w.threadId, -1);
    assert.strictEqual(w.stdin, null);
    assert.strictEqual(w.stdout, null);
    assert.strictEqual(w.stderr, null);
  }));
} else {
  process.exit(0);
}
