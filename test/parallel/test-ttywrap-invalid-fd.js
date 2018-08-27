// Flags: --expose-internals
'use strict';

const common = require('../common');
const tty = require('tty');
const { internalBinding } = require('internal/test/binding');
const {
  UV_EBADF,
  UV_EINVAL
} = internalBinding('uv');
const assert = require('assert');

assert.throws(
  () => new tty.WriteStream(-1),
  {
    code: 'ERR_INVALID_FD',
    name: 'RangeError [ERR_INVALID_FD]',
    message: '"fd" must be a positive integer: -1'
  }
);

{
  const info = {
    code: common.isWindows ? 'EBADF' : 'EINVAL',
    message: common.isWindows ? 'bad file descriptor' : 'invalid argument',
    errno: common.isWindows ? UV_EBADF : UV_EINVAL,
    syscall: 'uv_tty_init'
  };

  const suffix = common.isWindows ?
    'EBADF (bad file descriptor)' : 'EINVAL (invalid argument)';
  const message = `TTY initialization failed: uv_tty_init returned ${suffix}`;

  assert.throws(
    () => {
      common.runWithInvalidFD((fd) => {
        new tty.WriteStream(fd);
      });
    }, {
      code: 'ERR_TTY_INIT_FAILED',
      name: 'SystemError [ERR_TTY_INIT_FAILED]',
      message,
      info
    }
  );

  assert.throws(
    () => {
      common.runWithInvalidFD((fd) => {
        new tty.ReadStream(fd);
      });
    }, {
      code: 'ERR_TTY_INIT_FAILED',
      name: 'SystemError [ERR_TTY_INIT_FAILED]',
      message,
      info
    });
}

assert.throws(
  () => new tty.ReadStream(-1),
  {
    code: 'ERR_INVALID_FD',
    name: 'RangeError [ERR_INVALID_FD]',
    message: '"fd" must be a positive integer: -1'
  }
);
