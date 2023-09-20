'use strict';

require('../common');

const vm = require('vm');
const assert = require('assert');

let base = {
  propBase: 1
};

let sandbox = Object.create(base, {
  propSandbox: { value: 3 }
});

const context = vm.createContext(sandbox);

let result = vm.runInContext('Object.hasOwnProperty(this, "propBase");',
                             context);

assert.strictEqual(result, false);

// Ref: https://github.com/nodejs/node/issues/5350
base = { __proto__: null };
base.x = 1;
base.y = 2;

sandbox = { __proto__: base };
sandbox.z = 3;

assert.deepStrictEqual(Object.keys(sandbox), ['z']);

const code = 'x = 0; z = 4;';
result = vm.runInNewContext(code, sandbox);
assert.strictEqual(result, 4);

// Check that y is not an own property.
assert.deepStrictEqual(Object.keys(sandbox), ['z', 'x']);
