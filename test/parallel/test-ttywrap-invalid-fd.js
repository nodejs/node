'use strict';
// Flags: --expose-internals

const common = require('../common');
const tty = require('tty');
const { SystemError } = require('internal/errors');
const uv = process.binding('uv');

common.expectsError(
  () => new tty.WriteStream(-1),
  {
    code: 'ERR_INVALID_FD',
    type: RangeError,
    message: '"fd" must be a positive integer: -1'
  }
);

{
  const info = {
    code: common.isWindows ? 'EBADF' : 'EINVAL',
    message: common.isWindows ? 'bad file descriptor' : 'invalid argument',
    errno: common.isWindows ? uv.UV_EBADF : uv.UV_EINVAL,
    syscall: 'uv_tty_init'
  };

  const suffix = common.isWindows ?
    'EBADF (bad file descriptor)' : 'EINVAL (invalid argument)';
  const message = `TTY initialization failed: uv_tty_init returns ${suffix}`;

  common.expectsError(
    () => {
      common.runWithInvalidFD((fd) => {
        new tty.WriteStream(fd);
      });
    }, {
      code: 'ERR_TTY_INIT_FAILED',
      type: SystemError,
      message,
      info
    }
  );

  common.expectsError(
    () => {
      common.runWithInvalidFD((fd) => {
        new tty.ReadStream(fd);
      });
    }, {
      code: 'ERR_TTY_INIT_FAILED',
      type: SystemError,
      message,
      info
    });
}

common.expectsError(
  () => new tty.ReadStream(-1),
  {
    code: 'ERR_INVALID_FD',
    type: RangeError,
    message: '"fd" must be a positive integer: -1'
  }
);
