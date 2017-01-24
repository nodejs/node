'use strict';
require('../common');
const assert = require('assert');

const vm = require('vm');
const ctx = vm.createContext(global);

assert.doesNotThrow(function() {
  vm.runInContext('!function() { var x = console.log; }()', ctx);
});
