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
const util = require('util');

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

  test('test-abc', true, 'test-abc');
  test('test-a', false, 'test-abc');
  test('test-*', true, 'test-abc');
  test('test-*c', true, 'test-abc');
  test('test-*abc', true, 'test-abc');
  test('abc-test', true, 'abc-test');
  test('a*-test', true, 'abc-test');
  test('*-test', true, 'abc-test');
}

function test(environ, shouldWrite, section, forceColors = false) {
  let expectErr = '';
  const expectOut = shouldWrite ? 'enabled\n' : 'disabled\n';

  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child', section], {
    env: Object.assign(process.env, {
      NODE_DEBUG: environ,
      FORCE_COLOR: forceColors ? 'true' : 'false'
    })
  });

  if (shouldWrite) {
    if (forceColors) {
      const { colors, styles } = util.inspect;
      const addCodes = (arr) => [`\x1B[${arr[0]}m`, `\x1B[${arr[1]}m`];
      const num = addCodes(colors[styles.number]);
      const str = addCodes(colors[styles.string]);
      const regexp = addCodes(colors[styles.regexp]);
      const start = `${section.toUpperCase()} ${num[0]}${child.pid}${num[1]}`;
      const debugging = `${regexp[0]}/debugging/${regexp[1]}`;
      expectErr =
        `${start}: this { is: ${str[0]}'a'${str[1]} } ${debugging}\n` +
        `${start}: num=1 str=a obj={"foo":"bar"}\n`;
    } else {
      const start = `${section.toUpperCase()} ${child.pid}`;
      expectErr =
        `${start}: this { is: 'a' } /debugging/\n` +
        `${start}: num=1 str=a obj={"foo":"bar"}\n`;
    }
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
    // Run the test again, this time with colors enabled.
    if (!forceColors) {
      test(environ, shouldWrite, section, true);
    }
  }));
}


function child(section) {
  const tty = require('tty');
  // Make sure we check for colors, no matter of the stream's default.
  Object.defineProperty(process.stderr, 'hasColors', {
    value: tty.WriteStream.prototype.hasColors
  });
  // eslint-disable-next-line no-restricted-syntax
  const debug = util.debuglog(section, common.mustCall((cb) => {
    assert.strictEqual(typeof cb, 'function');
  }));
  debug('this', { is: 'a' }, /debugging/);
  debug('num=%d str=%s obj=%j', 1, 'a', { foo: 'bar' });
  console.log(debug.enabled ? 'enabled' : 'disabled');
}
