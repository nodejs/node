'use strict';
const common = require('../../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const bindingPath = require.resolve(`./build/${common.buildType}/binding`);
const binding = require(bindingPath);
assert.strictEqual(binding.hello(), 'world');
console.log('binding.hello() =', binding.hello());

// Test multiple loading of the same module.
delete require.cache[bindingPath];
const rebinding = require(bindingPath);
assert.strictEqual(rebinding.hello(), 'world');
assert.notStrictEqual(binding.hello, rebinding.hello);

// Test that workers can load addons declared using NAPI_MODULE_INIT().
new Worker(`
const { parentPort } = require('worker_threads');
const msg = require(${JSON.stringify(bindingPath)}).hello();
parentPort.postMessage(msg)`, { eval: true })
  .on('message', common.mustCall((msg) => assert.strictEqual(msg, 'world')));
