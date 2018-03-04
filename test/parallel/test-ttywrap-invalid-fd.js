'use strict';
// Flags: --expose-internals

const common = require('../common');
const tty = require('tty');
const { SystemError } = require('internal/errors');

common.expectsError(
  () => new tty.WriteStream(-1),
  {
    code: 'ERR_INVALID_FD',
    type: RangeError,
    message: '"fd" must be a positive integer: -1',
  }
);

{
  const message = common.isWindows ?
    'bad file descriptor: EBADF [uv_tty_init]' :
    'invalid argument: EINVAL [uv_tty_init]';

  common.expectsError(
    () => {
      common.runWithInvalidFD((fd) => {
        new tty.WriteStream(fd);
      });
    }, {
      code: 'ERR_SYSTEM_ERROR',
      type: SystemError,
      message,
    }
  );

  common.expectsError(
    () => {
      common.runWithInvalidFD((fd) => {
        new tty.ReadStream(fd);
      });
    }, {
      code: 'ERR_SYSTEM_ERROR',
      type: SystemError,
      message,
    });
}

common.expectsError(
  () => new tty.ReadStream(-1),
  {
    code: 'ERR_INVALID_FD',
    type: RangeError,
    message: '"fd" must be a positive integer: -1',
  }
);
