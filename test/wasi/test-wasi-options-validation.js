'use strict';

// Flags: --experimental-wasi-unstable-preview0

require('../common');
const assert = require('assert');
const { WASI } = require('wasi');

// If args is undefined, it should default to [] and should not throw.
new WASI({});

// If args is not an Array and not undefined, it should throw.
assert.throws(() => { new WASI({ args: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bargs\b/ });

// If env is not an Object and not undefined, it should throw.
assert.throws(() => { new WASI({ env: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\benv\b/ });

// If preopens is not an Object and not undefined, it should throw.
assert.throws(() => { new WASI({ preopens: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bpreopens\b/ });
