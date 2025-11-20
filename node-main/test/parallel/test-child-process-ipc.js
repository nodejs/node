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

const {
  mustCall,
  mustNotCall,
} = require('../common');
const assert = require('assert');
const debug = require('util').debuglog('test');

const { spawn } = require('child_process');
const fixtures = require('../common/fixtures');

const sub = fixtures.path('echo.js');

const child = spawn(process.argv[0], [sub]);

child.stderr.on('data', mustNotCall());

child.stdout.setEncoding('utf8');

const messages = [
  'hello world\r\n',
  'echo me\r\n',
];

child.stdout.on('data', mustCall((data) => {
  debug(`child said: ${JSON.stringify(data)}`);
  const test = messages.shift();
  debug(`testing for '${test}'`);
  assert.strictEqual(data, test);
  if (messages.length) {
    debug(`writing '${messages[0]}'`);
    child.stdin.write(messages[0]);
  } else {
    assert.strictEqual(messages.length, 0);
    child.stdin.end();
  }
}, messages.length));

child.stdout.on('end', mustCall((data) => {
  debug('child end');
}));
