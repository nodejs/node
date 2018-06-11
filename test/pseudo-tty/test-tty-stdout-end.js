'use strict';
const common = require('../common');

process.on('uncaughtException', common.mustCall((err) => {
  common.expectsError({
    code: 'ERR_STDOUT_CLOSE',
    type: Error,
    message: 'process.stdout cannot be closed'
  })(err);
}));

process.stdout.end();
