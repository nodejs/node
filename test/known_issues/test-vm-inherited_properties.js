'use strict';
// Ref: https://github.com/nodejs/node/issues/5350

require('../common');
const vm = require('vm');
const assert = require('assert');

const base = {
  propBase: 1
};

const sandbox = Object.create(base, {
  propSandbox: { value: 3 }
});

const context = vm.createContext(sandbox);

const result = vm.runInContext('this.hasOwnProperty("propBase");', context);

assert.strictEqual(result, false);
