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
const assert = require('assert');
const fs = require('fs');
const path = require('path');

// check for existence
assert(process.hasOwnProperty('config'));

// ensure that `process.config` is an Object
assert.strictEqual(Object(process.config), process.config);

const configPath = path.resolve(__dirname, '..', '..', 'config.gypi');
if (!fs.existsSync(configPath))
  common.skip(`'config.gypi' not found (${configPath})`);

const configRaw = fs.readFileSync(configPath, 'utf8')
  // clean up comment at the first line
  .split('\n').slice(1).join('\n')
  // normalize JSON
  .replace(/'/g, '"').replace(/"(true|false)"/g, '$1');

const config = JSON.parse(configRaw);

if (config.variables.node_target_type === 'static_library')
  common.skip(`'process.config' not relevant when building as static library`);

assert.deepStrictEqual(config, process.config);
