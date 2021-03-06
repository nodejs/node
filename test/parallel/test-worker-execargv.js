'use strict';
const common = require('../common');
const assert = require('assert');

// This test ensures that Workers have the ability to get
// their own command line flags.

const { Worker } = require('worker_threads');
const { StringDecoder } = require('string_decoder');
const decoder = new StringDecoder('utf8');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename, { execArgv: ['--trace-warnings'] });
  w.stderr.on('data', common.mustCall((chunk) => {
    const error = decoder.write(chunk);
    assert.ok(
      /Warning: some warning[\s\S]*at Object\.<anonymous>/.test(error)
    );
  }));

  new Worker(
    "require('worker_threads').parentPort.postMessage(process.execArgv)",
    { eval: true, execArgv: ['--trace-warnings'] })
    .on('message', common.mustCall((data) => {
      assert.deepStrictEqual(data, ['--trace-warnings']);
    }));
} else {
  process.emitWarning('some warning');
  assert.deepStrictEqual(process.execArgv, ['--trace-warnings']);
}
