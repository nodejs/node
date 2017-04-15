'use strict';
// Refs: https://github.com/nodejs/node/issues/10223

require('../common');
const vm = require('vm');
const assert = require('assert');

const context = vm.createContext({});

const code = `
   Object.defineProperty(this, 'foo', {value: 5});
   Object.getOwnPropertyDescriptor(this, 'foo');
`;

const desc = vm.runInContext(code, context);

assert.strictEqual(desc.writable, false);
