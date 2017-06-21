// Flags: --pending-deprecation --no-warnings
'use strict';

const common = require('../common');

const bufferWarning = 'The Buffer() and new Buffer() constructors are not ' +
                      'recommended for use due to security and usability ' +
                      'concerns. Please use the new Buffer.alloc(), ' +
                      'Buffer.allocUnsafe(), or Buffer.from() construction ' +
                      'methods instead.';

common.expectWarning('DeprecationWarning', bufferWarning);

// This is used to make sure that a warning is only emitted once even though
// `new Buffer()` is called twice.
process.on('warning', common.mustCall());

new Buffer(10);

new Buffer(10);
