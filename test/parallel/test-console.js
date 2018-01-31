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
assert.strictEqual(typeof process.stdout.fd, 'number');
assert.strictEqual(typeof process.stderr.fd, 'number');

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

// Check that the `Error` is a `TypeError` but do not check the message as it
// will be different in different JavaScript engines.
assert.throws(() => console.time(Symbol('test')),
              TypeError);
assert.throws(() => console.timeEnd(Symbol('test')),
              TypeError);


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
console.log({ slashes: '\\\\' });
console.log(custom_inspect);

// test console.debug() goes to stdout
console.debug('foo');
console.debug('foo', 'bar');
console.debug('%s %s', 'foo', 'bar', 'hop');
console.debug({ slashes: '\\\\' });
console.debug(custom_inspect);

// test console.info() goes to stdout
console.info('foo');
console.info('foo', 'bar');
console.info('%s %s', 'foo', 'bar', 'hop');
console.info({ slashes: '\\\\' });
console.info(custom_inspect);

// test console.error() goes to stderr
console.error('foo');
console.error('foo', 'bar');
console.error('%s %s', 'foo', 'bar', 'hop');
console.error({ slashes: '\\\\' });
console.error(custom_inspect);

// test console.warn() goes to stderr
console.warn('foo');
console.warn('foo', 'bar');
console.warn('%s %s', 'foo', 'bar', 'hop');
console.warn({ slashes: '\\\\' });
console.warn(custom_inspect);

// test console.dir()
console.dir(custom_inspect);
console.dir(custom_inspect, { showHidden: false });
console.dir({ foo: { bar: { baz: true } } }, { depth: 0 });
console.dir({ foo: { bar: { baz: true } } }, { depth: 1 });

// test console.dirxml()
console.dirxml(custom_inspect, custom_inspect);
console.dirxml(
  { foo: { bar: { baz: true } } },
  { foo: { bar: { quux: false } } },
  { foo: { bar: { quux: true } } }
);

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

// verify that values are coerced to strings
console.time([]);
console.timeEnd([]);
console.time({});
console.timeEnd({});
console.time(null);
console.timeEnd(null);
console.time(undefined);
console.timeEnd('default');
console.time('default');
console.timeEnd();
console.time(NaN);
console.timeEnd(NaN);

assert.doesNotThrow(() => {
  console.assert(false, '%s should', 'console.assert', 'not throw');
  assert.strictEqual(errStrings[errStrings.length - 1],
                     'Assertion failed: console.assert should not throw\n');
});

assert.doesNotThrow(() => {
  console.assert(true, 'this should not throw');
});

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
  assert.strictEqual(strings.shift(), `${expected}\n`);
  assert.strictEqual(errStrings.shift(), `${expected}\n`);
}

for (const expected of expectedStrings) {
  assert.strictEqual(strings.shift(), `${expected}\n`);
  assert.strictEqual(errStrings.shift(), `${expected}\n`);
}

for (const expected of expectedStrings) {
  assert.strictEqual(strings.shift(), `${expected}\n`);
}

assert.strictEqual(strings.shift(),
                   "{ foo: 'bar', inspect: [Function: inspect] }\n");
assert.strictEqual(strings.shift(),
                   "{ foo: 'bar', inspect: [Function: inspect] }\n");
assert.ok(strings.shift().includes('foo: [Object]'));
assert.strictEqual(strings.shift().includes('baz'), false);
assert.strictEqual(strings.shift(), 'inspect inspect\n');
assert.ok(strings[0].includes('foo: { bar: { baz:'));
assert.ok(strings[0].includes('quux'));
assert.ok(strings.shift().includes('quux: true'));

assert.ok(/^label: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^__proto__: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^constructor: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^hasOwnProperty: \d+\.\d{3}ms$/.test(strings.shift().trim()));

// verify that console.time() coerces label values to strings as expected
assert.ok(/^: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^\[object Object\]: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^null: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^default: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^default: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^NaN: \d+\.\d{3}ms$/.test(strings.shift().trim()));

assert.strictEqual(errStrings.shift().split('\n').shift(),
                   'Trace: This is a {"formatted":"trace"} 10 foo');

// hijack stderr to catch `process.emitWarning` which is using
// `process.nextTick`
common.hijackStderr(common.mustCall(function(data) {
  common.restoreStderr();

  // stderr.write will catch sync error, so use `process.nextTick` here
  process.nextTick(function() {
    assert.strictEqual(data.includes('no such label'), true);
  });
}));
