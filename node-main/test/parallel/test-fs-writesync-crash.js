'use strict';

require('../common');

const {
  writeSync,
  writeFileSync,
  chmodSync,
  openSync,
} = require('node:fs');

const {
  throws,
} = require('node:assert');

// If a file's mode change after it is opened but before it is written to,
// and the Object.prototype is manipulated to throw an error when the errno
// or fd property is set or accessed, then the writeSync call would crash
// the process. This test verifies that the error is properly propagated
// instead.

const tmpdir = require('../common/tmpdir');
console.log(tmpdir.path);
tmpdir.refresh();
const path = `${tmpdir.path}/foo`;
writeFileSync(path, '');

// Do this after calling tmpdir.refresh() or that call will fail
// before we get to the part we want to test.
const error = new Error();
Object.defineProperty(Object.prototype, 'errno', {
  __proto__: null,
  set() {
    throw error;
  },
  get() { return 0; }
});

const fd = openSync(path);
chmodSync(path, 0o600);

throws(() => writeSync(fd, 'test'), error);
