'use strict';

const { Worker, isMainThread, parentPort } = require('worker_threads');
const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { createRequire } = require('module');

const loadFixture = createRequire(fixtures.path('node_modules'));

if (isMainThread) {
  [[], ['--no-addons']].map((execArgv) => {
    const worker = new Worker(__filename, { execArgv });

    worker.on('message', common.mustCall((message) => {
      if (execArgv.length === 0) {
        assert.strictEqual(message, 'using native addons');
      } else {
        assert.strictEqual(message, 'not using native addons');
      }
    }));
  });
} else {
  const message = loadFixture('pkgexports/no-addons');
  parentPort.postMessage(message);
}
