// Flags: --pending-deprecation --no-warnings
'use strict';

const common = require('../common');
const Buffer = require('buffer').Buffer;

const bufferWarning = 'The Buffer() and new Buffer() constructors are not ' +
                      'recommended for use due to security and usability ' +
                      'concerns. Please use the new Buffer.alloc(), ' +
                      'Buffer.allocUnsafe(), or Buffer.from() construction ' +
                      'methods instead.';

common.expectWarning('DeprecationWarning', bufferWarning);

new Buffer(10);
