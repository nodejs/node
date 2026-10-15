'use strict';
// Regression test for https://github.com/nodejs/node/issues/64155.
// V8 used to copy the value of a //# sourceMappingURL= magic comment onto
// the heap while parsing, so a large inline source map aborted the process
// when loaded in a worker with a small heap, although the (external) source
// itself fits. The value is now a substring of the source.
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const { Worker } = require('worker_threads');

tmpdir.refresh();
const workerPath = tmpdir.resolve('worker.cjs');
fs.writeFileSync(workerPath,
                 "require('worker_threads').parentPort.postMessage('loaded');\n" +
                 '//# sourceMappingURL=data:application/json;base64,' +
                 'A'.repeat(32 * 1024 * 1024) + '\n');

const worker = new Worker(workerPath, {
  resourceLimits: { maxOldGenerationSizeMb: 16 },
});
worker.on('message', common.mustCall((msg) => assert.strictEqual(msg, 'loaded')));
worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
