// Flags: --expose-internals
'use strict';

const common = require('../common');

const internalProcess = require('internal/process');

const errMsg = 'setup_cpuUsage failed';
process.cpuUsage = () => { throw new Error(errMsg); };

internalProcess.setup_cpuUsage();

common.expectsError(() => process.cpuUsage(), {
  type: Error,
  message: errMsg
});
