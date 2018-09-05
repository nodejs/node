'use strict';
const common = require('../common');
require('../common/common-tty');

process.on('uncaughtException', common.expectsError({
  code: 'ERR_STDOUT_CLOSE',
  type: Error,
  message: 'process.stdout cannot be closed'
}));

process.stdout.end();
