'use strict';

// Sandbox throws in CopyProperties() despite no code being run
// Issue: https://github.com/nodejs/node/issues/11902


require('../common');
const assert = require('assert');
const vm = require('vm');

const handler = {
  getOwnPropertyDescriptor: (target, prop) => {
    throw new Error('whoops');
  }
};
const sandbox = new Proxy({ foo: 'bar' }, handler);
const context = vm.createContext(sandbox);


assert.doesNotThrow(() => vm.runInContext('', context));
