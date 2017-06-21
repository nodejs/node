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

assert.ok(process.stdout.writable);
assert.ok(process.stderr.writable);
// Support legacy API
assert.strictEqual('number', typeof process.stdout.fd);
assert.strictEqual('number', typeof process.stderr.fd);

assert.doesNotThrow(function() {
  process.once('warning', common.mustCall((warning) => {
    assert(/no such label/.test(warning.message));
  }));

  console.timeEnd('no such label');
});

assert.doesNotThrow(function() {
  console.time('label');
  console.timeEnd('label');
});

// an Object with a custom .inspect() function
const custom_inspect = { foo: 'bar', inspect: () => 'inspect' };

const strings = [];
const errStrings = [];
common.hijackStdout(function(data) {
  strings.push(data);
});
common.hijackStderr(function(data) {
  errStrings.push(data);
});

// test console.log() goes to stdout
console.log('foo');
console.log('foo', 'bar');
console.log('%s %s', 'foo', 'bar', 'hop');
console.log({slashes: '\\\\'});
console.log(custom_inspect);

// test console.info() goes to stdout
console.info('foo');
console.info('foo', 'bar');
console.info('%s %s', 'foo', 'bar', 'hop');
console.info({slashes: '\\\\'});
console.info(custom_inspect);

// test console.error() goes to stderr
console.error('foo');
console.error('foo', 'bar');
console.error('%s %s', 'foo', 'bar', 'hop');
console.error({slashes: '\\\\'});
console.error(custom_inspect);

// test console.warn() goes to stderr
console.warn('foo');
console.warn('foo', 'bar');
console.warn('%s %s', 'foo', 'bar', 'hop');
console.warn({slashes: '\\\\'});
console.warn(custom_inspect);

// test console.dir()
console.dir(custom_inspect);
console.dir(custom_inspect, { showHidden: false });
console.dir({ foo: { bar: { baz: true } } }, { depth: 0 });
console.dir({ foo: { bar: { baz: true } } }, { depth: 1 });

// test console.trace()
console.trace('This is a %j %d', { formatted: 'trace' }, 10, 'foo');

// test console.time() and console.timeEnd() output
console.time('label');
console.timeEnd('label');

// verify that Object.prototype properties can be used as labels
console.time('__proto__');
console.timeEnd('__proto__');
console.time('constructor');
console.timeEnd('constructor');
console.time('hasOwnProperty');
console.timeEnd('hasOwnProperty');

assert.strictEqual(strings.length, process.stdout.writeTimes);
assert.strictEqual(errStrings.length, process.stderr.writeTimes);
common.restoreStdout();
common.restoreStderr();

// verify that console.timeEnd() doesn't leave dead links
const timesMapSize = console._times.size;
console.time('label1');
console.time('label2');
console.time('label3');
console.timeEnd('label1');
console.timeEnd('label2');
console.timeEnd('label3');
assert.strictEqual(console._times.size, timesMapSize);

const expectedStrings = [
  'foo', 'foo bar', 'foo bar hop', "{ slashes: '\\\\\\\\' }", 'inspect'
];

for (const expected of expectedStrings) {
  assert.strictEqual(`${expected}\n`, strings.shift());
  assert.strictEqual(`${expected}\n`, errStrings.shift());
}

for (const expected of expectedStrings) {
  assert.strictEqual(`${expected}\n`, strings.shift());
  assert.strictEqual(`${expected}\n`, errStrings.shift());
}

assert.strictEqual("{ foo: 'bar', inspect: [Function: inspect] }\n",
                   strings.shift());
assert.strictEqual("{ foo: 'bar', inspect: [Function: inspect] }\n",
                   strings.shift());
assert.ok(strings.shift().includes('foo: [Object]'));
assert.strictEqual(strings.shift().includes('baz'), false);
assert.ok(/^label: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^__proto__: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^constructor: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^hasOwnProperty: \d+\.\d{3}ms$/.test(strings.shift().trim()));

assert.strictEqual('Trace: This is a {"formatted":"trace"} 10 foo',
                   errStrings.shift().split('\n').shift());

assert.throws(() => {
  console.assert(false, 'should throw');
}, common.expectsError({
  code: 'ERR_ASSERTION',
  message: /^should throw$/
}));

assert.doesNotThrow(() => {
  console.assert(true, 'this should not throw');
});

// hijack stderr to catch `process.emitWarning` which is using
// `process.nextTick`
common.hijackStderr(common.mustCall(function(data) {
  common.restoreStderr();

  // stderr.write will catch sync error, so use `process.nextTick` here
  process.nextTick(function() {
    assert.strictEqual(data.includes('no such label'), true);
  });
}));
