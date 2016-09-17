'use strict';
const common = require('../common');

const expected =
  'Using Buffer without `new` will soon stop working. ' +
  'Use `new Buffer()`, or preferably ' +
  '`Buffer.from()`, `Buffer.allocUnsafe()` or `Buffer.alloc()` instead.';
common.expectWarning('DeprecationWarning', expected);

Buffer(1);
Buffer(1);
