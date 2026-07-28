'use strict';

require('../common');
const assert = require('assert');
const { WASI } = require('wasi');

// If args is undefined, it should default to [] and should not throw.
new WASI({ version: 'preview1' });

// If args is not an Array and not undefined, it should throw.
assert.throws(() => { new WASI({ version: 'preview1', args: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bargs\b/ });

// If env is not an Object and not undefined, it should throw.
assert.throws(() => { new WASI({ version: 'preview1', env: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\benv\b/ });

// If preopens is not an Object and not undefined, it should throw.
assert.throws(() => { new WASI({ version: 'preview1', preopens: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bpreopens\b/ });

// If returnOnExit is not a boolean and not undefined, it should throw.
assert.throws(() => { new WASI({ version: 'preview1', returnOnExit: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\breturnOnExit\b/ });

// If stdin is not an int32 and not undefined, it should throw.
assert.throws(() => { new WASI({ version: 'preview1', stdin: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bstdin\b/ });

// If stdout is not an int32 and not undefined, it should throw.
assert.throws(() => { new WASI({ version: 'preview1', stdout: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bstdout\b/ });

// If stderr is not an int32 and not undefined, it should throw.
assert.throws(() => { new WASI({ version: 'preview1', stderr: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bstderr\b/ });

// If options is provided, but not an object, the constructor should throw.
[null, 'foo', '', 0, NaN, Symbol(), true, false, () => {}].forEach((value) => {
  assert.throws(() => { new WASI(value); },
                { code: 'ERR_INVALID_ARG_TYPE' });
});

// Verify that exceptions thrown from the binding layer are handled.
assert.throws(() => {
  new WASI({ version: 'preview1', preopens: { '/sandbox': '__/not/real/path' } });
}, { code: 'UVWASI_ENOENT', message: /uvwasi_init/ });

// If version is not a string, it should throw
assert.throws(() => { new WASI({ version: { x: 'y' } }); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bversion\b/ });

// If version is not specified, it should throw.
assert.throws(() => { new WASI(); },
              { code: 'ERR_INVALID_ARG_TYPE', message: /\bversion\b/ });

// If version is an unsupported version, it should throw
assert.throws(() => { new WASI({ version: 'not_a_version' }); },
              { code: 'ERR_INVALID_ARG_VALUE', message: /\bversion\b/ });
