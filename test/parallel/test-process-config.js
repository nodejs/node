'use strict';

const common = require('../common');

// Checks that the internal process.config is equivalent to the config.gypi file
// created when we run configure.

const assert = require('assert');
const fs = require('fs');
const path = require('path');

// Check for existence of `process.config`.
assert(process.hasOwnProperty('config'));

// Ensure that `process.config` is an Object.
assert.strictEqual(Object(process.config), process.config);

const configPath = path.resolve(__dirname, '..', '..', 'config.gypi');

if (!fs.existsSync(configPath)) {
  common.skip('config.gypi does not exist.');
}

let config = fs.readFileSync(configPath, 'utf8');

// Clean up comment at the first line.
config = config.split('\n').slice(1).join('\n');
config = config.replace(/"/g, '\\"');
config = config.replace(/'/g, '"');
config = JSON.parse(config, function(key, value) {
  if (value === 'true') return true;
  if (value === 'false') return false;
  return value;
});

try {
  assert.deepStrictEqual(config, process.config);
} catch (e) {
  // If the assert fails, it only shows 3 lines. We need all the output to
  // compare.
  console.log('config:', config);
  console.log('process.config:', process.config);

  throw e;
}
