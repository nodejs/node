'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

assert.doesNotThrow(function() {
  var context = vm.createContext({ process: process });
  var result = vm.runInContext('process.env["PATH"]', context);
  assert.notEqual(undefined, result);
});
