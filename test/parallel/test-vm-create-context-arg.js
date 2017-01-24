'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

assert.throws(function() {
  vm.createContext('string is not supported');
}, TypeError);

assert.doesNotThrow(function() {
  vm.createContext({ a: 1 });
  vm.createContext([0, 1, 2, 3]);
});

assert.doesNotThrow(function() {
  const sandbox = {};
  vm.createContext(sandbox);
  vm.createContext(sandbox);
});
