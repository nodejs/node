// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { internalBinding } = require('internal/test/binding');

const binding = internalBinding('fs');

const badFd = fs.openSync(__filename, 'r');
fs.closeSync(badFd);

// The synchronous fstat binding honours its do_not_throw_error argument.
assert.strictEqual(binding.fstat(badFd, false, undefined, true), undefined);
assert.throws(() => binding.fstat(badFd, false, undefined, false), {
  code: 'EBADF',
  syscall: 'fstat',
});

// readFileSync() surfaces the fstat error and closes a file descriptor it
// opened itself, but leaves a user-supplied one alone.
fs.openSync = () => badFd;
fs.closeSync = common.mustCall((fd) => {
  assert.strictEqual(fd, badFd);
});

assert.throws(() => fs.readFileSync('dummy', 'latin1'), {
  code: 'EBADF',
  syscall: 'fstat',
});
assert.throws(() => fs.readFileSync(badFd, 'latin1'), {
  code: 'EBADF',
  syscall: 'fstat',
});
