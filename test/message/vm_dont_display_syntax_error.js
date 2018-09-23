'use strict';
var common = require('../common');
var assert = require('assert');
var vm = require('vm');

console.error('beginning');

try {
  vm.runInThisContext('var 5;', {
    filename: 'test.vm',
    displayErrors: false
  });
} catch (e) {}

console.error('middle');

vm.runInThisContext('var 5;', {
  filename: 'test.vm',
  displayErrors: false
});

console.error('end');
