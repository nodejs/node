'use strict';

require('../common');
const assert = require('assert');

// no args
assert.throws(
  () => { assert.fail(); },
  /^AssertionError: undefined undefined undefined$/
);

// one arg = message
assert.throws(
  () => { assert.fail('custom message'); },
  /^AssertionError: custom message$/
);

// two args only, operator defaults to '!='
assert.throws(
  () => { assert.fail('first', 'second'); },
  /^AssertionError: 'first' != 'second'$/
);

// three args
assert.throws(
  () => { assert.fail('ignored', 'ignored', 'another custom message'); },
  /^AssertionError: another custom message$/
);

// no third arg (but a fourth arg)
assert.throws(
  () => { assert.fail('first', 'second', undefined, 'operator'); },
  /^AssertionError: 'first' operator 'second'$/
);

// The stackFrameFunction should exclude the foo frame
assert.throws(
  function foo() { assert.fail('first', 'second', 'message', '!==', foo); },
  (err) => !/foo/m.test(err.stack)
);
