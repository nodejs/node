'use strict';
const common = require('../../common');

if (common.isWindows)
  common.skip('dlopen global symbol loading is not supported on this os.');

const assert = require('assert');
const { Worker } = require('worker_threads');

// Check that modules that are not declared as context-aware cannot be re-loaded
// from workers.

const bindingPath = require.resolve(`./build/${common.buildType}/binding`);
require(bindingPath);

new Worker(`require(${JSON.stringify(bindingPath)})`, { eval: true })
  .on('error', common.mustCall((err) => {
    assert.strictEqual(err.constructor, Error);
    assert.strictEqual(err.message,
                       `Module did not self-register: '${bindingPath}'.`);
  }));
