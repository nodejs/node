'use strict';
require('../common');

// Setting __proto__ on vm context's globalThis should not cause a crash
// Regression test for https://github.com/nodejs/node/issues/47798

const vm = require('vm');
const context = vm.createContext();

const contextGlobalThis = vm.runInContext('this', context);

// Should not crash.
contextGlobalThis.__proto__ = null; // eslint-disable-line no-proto
