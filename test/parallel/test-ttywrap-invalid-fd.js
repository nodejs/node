'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tty = require('tty');

assert.throws(() => {
  new tty.WriteStream(-1);
}, common.expectsError({
  code: 'ERR_INVALID_FD',
  type: RangeError,
  message: '"fd" must be a positive integer: -1'
})
);

const err_regex = common.isWindows ?
  /^Error: EBADF: bad file descriptor, uv_tty_init$/ :
  /^Error: EINVAL: invalid argument, uv_tty_init$/;
assert.throws(() => {
  let fd = 2;
  // Get first known bad file descriptor.
  try {
    while (fs.fstatSync(++fd));
  } catch (e) { }
  new tty.WriteStream(fd);
}, err_regex);

assert.throws(() => {
  new tty.ReadStream(-1);
}, common.expectsError({
  code: 'ERR_INVALID_FD',
  type: RangeError,
  message: '"fd" must be a positive integer: -1'
})
);

assert.throws(() => {
  let fd = 2;
  // Get first known bad file descriptor.
  try {
    while (fs.fstatSync(++fd));
  } catch (e) { }
  new tty.ReadStream(fd);
}, err_regex);
