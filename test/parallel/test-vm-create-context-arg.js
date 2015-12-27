'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');

assert.throws(function() {
  vm.createContext('string is not supported');
}, TypeError);

assert.doesNotThrow(function() {
  vm.createContext({ a: 1 });
  vm.createContext([0, 1, 2, 3]);
});

assert.doesNotThrow(function() {
  var sandbox = {};
  vm.createContext(sandbox);
  vm.createContext(sandbox);
});
