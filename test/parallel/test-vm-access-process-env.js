'use strict';
// Tests that node does neither crash nor throw an error when accessing
// process.env when inside a VM context.
// See https://github.com/nodejs/node-v0.x-archive/issues/7511.

require('../common');
const assert = require('assert');
const vm = require('vm');

assert.doesNotThrow(function() {
  const context = vm.createContext({ process: process });
  const result = vm.runInContext('process.env["PATH"]', context);
  assert.notStrictEqual(undefined, result);
});
