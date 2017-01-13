'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

assert.doesNotThrow(function() {
  const context = vm.createContext({ process: process });
  const result = vm.runInContext('process.env["PATH"]', context);
  assert.notStrictEqual(undefined, result);
});
