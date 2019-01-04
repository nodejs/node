'use strict';
const common = require('../common');
const assert = require('assert');

// This test ensures that Workers have the ability to get
// their own command line flags.

const { Worker, isMainThread } = require('worker_threads');
const { StringDecoder } = require('string_decoder');
const decoder = new StringDecoder('utf8');

if (isMainThread) {
  const w = new Worker(__filename, { execArgv: ['--trace-warnings'] });
  w.stderr.on('data', common.mustCall((chunk) => {
    const error = decoder.write(chunk);
    assert.ok(
      /Warning: some warning[\s\S]*at Object\.<anonymous>/.test(error)
    );
  }));
} else {
  process.emitWarning('some warning');
}
