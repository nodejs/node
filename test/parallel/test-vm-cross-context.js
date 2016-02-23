'use strict';
require('../common');
var assert = require('assert');

var vm = require('vm');
var ctx = vm.createContext(global);

assert.doesNotThrow(function() {
  vm.runInContext('!function() { var x = console.log; }()', ctx);
});
