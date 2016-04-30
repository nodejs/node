'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const path = require('path');
const Module = require('module');

let counter = 0;

Module._runInThisContext = function runInThisContext(source, options) {
  counter++;
  return vm.runInThisContext(source, options);
};

const empty = require(path.join(common.fixturesDir, 'empty.js'));

assert.equal(counter, 1);
