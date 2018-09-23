// Flags: --expose-internals

'use strict';

// This tests `internal/errors.useOriginalName`
// This testing feature is needed to allows us to assert the types of
// errors without using instanceof, which is necessary in WPT harness.
// Refs: https://github.com/nodejs/node/pull/22556

require('../common');
const assert = require('assert');
const errors = require('internal/errors');


errors.E('TEST_ERROR_1', 'Error for testing purposes: %s',
         Error);
{
  const err = new errors.codes.TEST_ERROR_1('test');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error [TEST_ERROR_1]');
}

{
  errors.useOriginalName = true;
  const err = new errors.codes.TEST_ERROR_1('test');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error');
}

{
  errors.useOriginalName = false;
  const err = new errors.codes.TEST_ERROR_1('test');
  assert(err instanceof Error);
  assert.strictEqual(err.name, 'Error [TEST_ERROR_1]');
}
