'use strict';

// Flags: --experimental-wasi-unstable-preview1

require('../common');
const assert = require('assert');
const { WASI } = require('wasi');

// If args is undefined, it should default to [] and should not throw.
new WASI({});

// If args is not an Array and not undefined, it should throw.
assert.throws(() => { new WASI({ args: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE' });

// If env is not an Object and not undefined, it should throw.
assert.throws(() => { new WASI({ env: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE' });

// If preopens is not an Object and not undefined, it should throw.
assert.throws(() => { new WASI({ preopens: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE' });

// If returnOnExit is not a boolean and not undefined, it should throw.
assert.throws(() => { new WASI({ returnOnExit: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE' });

// If stdin is not an int32 and not undefined, it should throw.
assert.throws(() => { new WASI({ stdin: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE' });

// If stdout is not an int32 and not undefined, it should throw.
assert.throws(() => { new WASI({ stdout: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE' });

// If stderr is not an int32 and not undefined, it should throw.
assert.throws(() => { new WASI({ stderr: 'fhqwhgads' }); },
              { code: 'ERR_INVALID_ARG_TYPE' });

// If options is provided, but not an object, the constructor should throw.
[null, 'foo', '', 0, NaN, Symbol(), true, false, () => {}].forEach((value) => {
  assert.throws(() => { new WASI(value); },
                { code: 'ERR_INVALID_ARG_TYPE' });
});

// Verify that exceptions thrown from the binding layer are handled.
assert.throws(() => {
  new WASI({ preopens: { '/sandbox': '__/not/real/path' } });
}, { code: 'UVWASI_ENOENT' });
