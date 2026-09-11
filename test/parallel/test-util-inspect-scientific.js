'use strict';

require('../common');
const assert = require('assert');
const util = require('util');

// Ensure scientific notation is not broken when numericSeparator is enabled
{
  const value = 1e-7;
  const result = util.inspect(value, { numericSeparator: true });

  assert.strictEqual(result, '1e-7');
}

// Another scientific notation case (positive exponent)
{
  const value = 1e+21;
  const result = util.inspect(value, { numericSeparator: true });

  assert.strictEqual(result, '1e+21');
}

// Control case: regular number should still apply numeric separators
{
  const value = 1000000;
  const result = util.inspect(value, { numericSeparator: true });

  assert.strictEqual(result, '1_000_000');
}
