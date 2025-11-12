// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const common = require('../common');

// Checks that the internal process.config is equivalent to the config.gypi file
// created when we run configure.

const assert = require('assert');
const fs = require('fs');
const path = require('path');

// Check for existence of `process.config`.
assert(Object.hasOwn(process, 'config'));

// Ensure that `process.config` is an Object.
assert.strictEqual(Object(process.config), process.config);

// Ensure that you can't change config values
assert.throws(() => { process.config.variables = 42; }, TypeError);

const configPath = path.resolve(__dirname, '..', '..', 'config.gypi');

if (!fs.existsSync(configPath)) {
  common.skip('config.gypi does not exist.');
}

let config = fs.readFileSync(configPath, 'utf8');

// Clean up comment at the first line.
config = config.split('\n').slice(1).join('\n');
config = config.replace(/"/g, '\\"');
config = config.replace(/'/g, '"');
config = JSON.parse(config, (key, value) => {
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
