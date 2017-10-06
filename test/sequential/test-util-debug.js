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

const [, , modeArgv, sectionArgv] = process.argv;

if (modeArgv === 'child')
  child(sectionArgv);
else
  parent();

function parent() {
  test('foo,tud,bar', true, 'tud');
  test('foo,tud', true, 'tud');
  test('tud,bar', true, 'tud');
  test('tud', true, 'tud');
  test('foo,bar', false, 'tud');
  test('', false, 'tud');

  test('###', true, '###');
  test('hi:)', true, 'hi:)');
  test('f$oo', true, 'f$oo');
  test('f$oo', false, 'f.oo');
  test('no-bar-at-all', false, 'bar');
}

function test(environ, shouldWrite, section) {
  let expectErr = '';
  const expectOut = 'ok\n';

  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child', section], {
    env: Object.assign(process.env, { NODE_DEBUG: environ })
  });

  if (shouldWrite) {
    expectErr =
      `${section.toUpperCase()} ${child.pid}: this { is: 'a' } /debugging/\n${
        section.toUpperCase()} ${child.pid}: num=1 str=a obj={"foo":"bar"}\n`;
  }

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


function child(section) {
  const util = require('util');
  const debug = util.debuglog(section);
  debug('this', { is: 'a' }, /debugging/);
  debug('num=%d str=%s obj=%j', 1, 'a', { foo: 'bar' });
  console.log('ok');
}
