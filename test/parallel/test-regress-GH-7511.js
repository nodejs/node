'use strict';
var assert = require('assert');
var vm     = require('vm');

assert.doesNotThrow(function() {
  var context = vm.createContext({ process: process });
  var result  = vm.runInContext('process.env["PATH"]', context);
  assert.notEqual(undefined, result);
});
