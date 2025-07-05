'use strict';

require('../common');
const assert = require('node:assert');
const util = require('node:util');

const content = `
  X = x
  Y = y

  [section-a]
  X = x_a

  [section-b]
  Y = y_b
  Z = z_b
`;

// Test that sections can be normally selected
{
  const parsedEnv = util.parseEnv(content, { sections: ['section-b'] });
  assert.deepStrictEqual(parsedEnv, { X: 'x', Y: 'y_b', Z: 'z_b' });
}

// Test that sections are ignored when omitted
{
  const parsedEnv = util.parseEnv(content);
  assert.deepStrictEqual(parsedEnv, { X: 'x', Y: 'y' });
}

// Test that passing an non-array sections value causes an ERR_INVALID_ARG_TYPE error to be thrown
{
  let error;
  try {
    util.parseEnv(content, { sections: () => console.log('this is invalid') });
  } catch (e) {
    error = e;
  }
  assert.strictEqual(error.code, 'ERR_INVALID_ARG_TYPE');
}

// Test that passing an array not containing all strings an ERR_INVALID_ARG_TYPE error to be thrown
{
  let error;
  try {
    util.parseEnv(content, { sections: ['a', 'b', 1, () => 2] });
  } catch (e) {
    error = e;
  }
  assert.strictEqual(error.code, 'ERR_INVALID_ARG_TYPE');
}
