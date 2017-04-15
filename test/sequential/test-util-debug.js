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

if (process.argv[2] === 'child')
  child();
else
  parent();

function parent() {
  test('foo,tud,bar', true);
  test('foo,tud', true);
  test('tud,bar', true);
  test('tud', true);
  test('foo,bar', false);
  test('', false);
}

function test(environ, shouldWrite) {
  let expectErr = '';
  if (shouldWrite) {
    expectErr = 'TUD %PID%: this { is: \'a\' } /debugging/\n' +
                'TUD %PID%: number=1234 string=asdf obj={"foo":"bar"}\n';
  }
  const expectOut = 'ok\n';

  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child'], {
    env: Object.assign(process.env, { NODE_DEBUG: environ })
  });

  expectErr = expectErr.split('%PID%').join(child.pid);

  let err = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (c) => {
    err += c;
  });

  let out = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (c) => {
    out += c;
  });

  child.on('close', common.mustCall((c) => {
    assert(!c);
    assert.strictEqual(err, expectErr);
    assert.strictEqual(out, expectOut);
  }));
}


function child() {
  const util = require('util');
  const debug = util.debuglog('tud');
  debug('this', { is: 'a' }, /debugging/);
  debug('number=%d string=%s obj=%j', 1234, 'asdf', { foo: 'bar' });
  console.log('ok');
}
