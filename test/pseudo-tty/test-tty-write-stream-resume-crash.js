'use strict';

const common = require('../common');
const { WriteStream } = require('tty');
const fd = common.getTTYfd();

// Calling resume on a tty.WriteStream should be a no-op
// Ref: https://github.com/nodejs/node/issues/21203

const stream = new WriteStream(fd);
stream.resume();

stream.on('error', common.expectsError({
  code: 'ERR_TTY_WRITABLE_NOT_READABLE',
  type: Error,
  message: 'The Writable side of a TTY is not Readable'
}));
