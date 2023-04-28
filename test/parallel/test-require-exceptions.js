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
require('../common');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

// A module with an error in it should throw
assert.throws(() => {
  require(fixtures.path('/throws_error'));
}, /^Error: blah$/);

// Requiring the same module again should throw as well
assert.throws(() => {
  require(fixtures.path('/throws_error'));
}, /^Error: blah$/);

// Requiring a module that does not exist should throw an
// error with its `code` set to MODULE_NOT_FOUND
assert.throws(
  () => require('/DOES_NOT_EXIST'),
  { code: 'MODULE_NOT_FOUND' }
);

assertExists('/module-require/not-found/trailingSlash.js');
assertExists('/module-require/not-found/node_modules/module1/package.json');
assert.throws(
  () => require('/module-require/not-found/trailingSlash'),
  { code: 'MODULE_NOT_FOUND' }
);

function assertExists(fixture) {
  assert(fs.existsSync(fixtures.path(fixture)));
}
