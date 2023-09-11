// Flags: --pending-deprecation --no-warnings
'use strict';

const common = require('../common');

const bufferWarning = 'Buffer() is deprecated due to security and usability ' +
                      'issues. Please use the Buffer.alloc(), ' +
                      'Buffer.allocUnsafe(), or Buffer.from() methods instead.';

common.expectWarning('DeprecationWarning', bufferWarning, 'DEP0005');

// This is used to make sure that a warning is only emitted once even though
// `new Buffer()` is called twice.
process.on('warning', common.mustCall());

new Buffer(10);

new Buffer(10);
