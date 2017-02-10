'use strict';
const common = require('../common');
const assert = require('assert');

assert.throws(() => process.stdout.end(),
              common.expectsError({
                code: 'ERR_STDOUT_CLOSE',
                type: Error,
                message: 'process.stdout cannot be closed'
              }));
