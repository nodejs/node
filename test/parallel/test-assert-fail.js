'use strict';

require('../common');
const assert = require('assert');

// The stackFrameFunction should exclude the foo frame
assert.throws(
  function foo() { assert.fail('first', 'second', 'message', '!==', foo); },
  (err) => !/foo/m.test(err.stack)
);
