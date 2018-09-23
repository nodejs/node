'use strict';
var common = require('../common'),
    assert = require('assert'),
    vm     = require('vm');

assert.doesNotThrow(function() {
  var context = vm.createContext({ process: process });
  var result  = vm.runInContext('process.env["PATH"]', context);
  assert.notEqual(undefined, result);
});
