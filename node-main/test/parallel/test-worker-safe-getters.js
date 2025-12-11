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

  const { stdin, stdout, stderr } = w;

  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);

    // `postMessage` should not throw after termination
    // (this mimics the browser behavior).
    w.postMessage('foobar');
    w.ref();
    w.unref();

    // Sanity check.
    assert.strictEqual(w.threadId, -1);
    assert.strictEqual(w.stdin, stdin);
    assert.strictEqual(w.stdout, stdout);
    assert.strictEqual(w.stderr, stderr);
  }));
} else {
  process.exit(0);
}
