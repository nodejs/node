'use strict';
require('../common');
const assert = require('assert');

// This checks that after accessing a missing env var, a subsequent
// env read will succeed even for empty variables.

{
  process.env.FOO = '';
  process.env.NONEXISTENT_ENV_VAR; // eslint-disable-line no-unused-expressions
  const foo = process.env.FOO;

  assert.strictEqual(foo, '');
}

{
  process.env.FOO = '';
  process.env.NONEXISTENT_ENV_VAR; // eslint-disable-line no-unused-expressions
  const hasFoo = 'FOO' in process.env;

  assert.strictEqual(hasFoo, true);
}
