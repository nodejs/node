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

// Changes in environment should be visible to child processes
if (process.argv[2] === 'you-are-the-child') {
  assert.strictEqual('NODE_PROCESS_ENV_DELETED' in process.env, false);
  assert.strictEqual(process.env.NODE_PROCESS_ENV, '42');
  assert.strictEqual(process.env.hasOwnProperty, 'asdf');
  const has = Object.hasOwn(process.env, 'hasOwnProperty');
  assert.strictEqual(has, true);
  process.exit(0);
}

{
  const spawn = require('child_process').spawn;

  assert.strictEqual(Object.prototype.hasOwnProperty,
                     process.env.hasOwnProperty);
  const has = Object.hasOwn(process.env, 'hasOwnProperty');
  assert.strictEqual(has, false);

  process.env.hasOwnProperty = 'asdf';

  process.env.NODE_PROCESS_ENV = 42;
  assert.strictEqual(process.env.NODE_PROCESS_ENV, '42');

  process.env.NODE_PROCESS_ENV_DELETED = 42;
  assert.strictEqual('NODE_PROCESS_ENV_DELETED' in process.env, true);

  delete process.env.NODE_PROCESS_ENV_DELETED;
  assert.strictEqual('NODE_PROCESS_ENV_DELETED' in process.env, false);

  const child = spawn(process.argv[0], [process.argv[1], 'you-are-the-child']);
  child.stdout.on('data', function(data) { console.log(data.toString()); });
  child.stderr.on('data', function(data) { console.log(data.toString()); });
  child.on('exit', function(statusCode) {
    if (statusCode !== 0) {
      process.exit(statusCode);  // Failed assertion in child process
    }
  });
}


// Delete should return true except for non-configurable properties
// https://github.com/nodejs/node/issues/7960
delete process.env.NON_EXISTING_VARIABLE;
assert(delete process.env.NON_EXISTING_VARIABLE);

// For the moment we are not going to support setting the timezone via the
// environment variables. The problem is that various V8 platform backends
// deal with timezone in different ways. The Windows platform backend caches
// the timezone value while the Linux one hits libc for every query.
//
// https://github.com/joyent/node/blob/08782931205bc4f6d28102ebc29fd806e8ccdf1f/deps/v8/src/platform-linux.cc#L339-345
// https://github.com/joyent/node/blob/08782931205bc4f6d28102ebc29fd806e8ccdf1f/deps/v8/src/platform-win32.cc#L590-596
//
// // set the timezone; see tzset(3)
// process.env.TZ = 'Europe/Amsterdam';
//
// // time difference between Greenwich and Amsterdam is +2 hours in the summer
// date = new Date('Fri, 10 Sep 1982 03:15:00 GMT');
// assert.strictEqual(3, date.getUTCHours());
// assert.strictEqual(5, date.getHours());

// Environment variables should be case-insensitive on Windows, and
// case-sensitive on other platforms.
process.env.TEST = 'test';
assert.strictEqual(process.env.TEST, 'test');

// Check both mixed case and lower case, to avoid any regressions that might
// simply convert input to lower case.
if (common.isWindows) {
  assert.strictEqual(process.env.test, 'test');
  assert.strictEqual(process.env.teST, 'test');
} else {
  assert.strictEqual(process.env.test, undefined);
  assert.strictEqual(process.env.teST, undefined);
}

{
  const keys = Object.keys(process.env);
  assert.ok(keys.length > 0);
}

// https://github.com/nodejs/node/issues/45380
{
  const env = structuredClone(process.env);
  // deepEqual(), not deepStrictEqual(), because of different prototypes.
  // eslint-disable-next-line no-restricted-properties
  assert.deepEqual(env, process.env);
}

// Setting environment variables on Windows with empty names should not cause
// an assertion failure.
// https://github.com/nodejs/node/issues/32920
{
  process.env[''] = '';
  assert.strictEqual(process.env[''], undefined);
}
