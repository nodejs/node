'use strict';
const common = require('../common');

process.on('uncaughtException', common.expectsError({
  code: 'ERR_STDOUT_CLOSE',
  type: Error,
  message: 'process.stdout cannot be closed'
}));

process.stdout.end();
