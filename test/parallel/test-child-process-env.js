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
  isWindows,
  mustCall,
  mustCallAtLeast,
} = require('../common');
const assert = require('assert');
const os = require('os');
const debug = require('util').debuglog('test');

const spawn = require('child_process').spawn;

const env = {
  ...process.env,
  'HELLO': 'WORLD',
  'UNDEFINED': undefined,
  'NULL': null,
  'EMPTY': '',
  'duplicate': 'lowercase',
  'DUPLICATE': 'uppercase',
};
Object.setPrototypeOf(env, {
  'FOO': 'BAR'
});

let child;
if (isWindows) {
  child = spawn('cmd.exe', ['/c', 'set'], { env });
} else {
  child = spawn('/usr/bin/env', [], { env });
}


let response = '';

child.stdout.setEncoding('utf8');

child.stdout.on('data', mustCallAtLeast((chunk) => {
  debug(`stdout: ${chunk}`);
  response += chunk;
}));

child.stdout.on('end', mustCall(() => {
  assert.ok(response.includes('HELLO=WORLD'));
  assert.ok(response.includes('FOO=BAR'));
  assert.ok(!response.includes('UNDEFINED=undefined'));
  assert.ok(response.includes('NULL=null'));
  assert.ok(response.includes(`EMPTY=${os.EOL}`));
  if (isWindows) {
    assert.ok(response.includes('DUPLICATE=uppercase'));
    assert.ok(!response.includes('duplicate=lowercase'));
  } else {
    assert.ok(response.includes('DUPLICATE=uppercase'));
    assert.ok(response.includes('duplicate=lowercase'));
  }
}));
