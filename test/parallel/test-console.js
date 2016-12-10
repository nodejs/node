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
const custom_inspect = { foo: 'bar', inspect: () => { return 'inspect'; } };

const stdout_write = global.process.stdout.write;
const stderr_write = global.process.stderr.write;
const strings = [];
const errStrings = [];
global.process.stdout.write = function(string) {
  strings.push(string);
};
global.process.stderr.write = function(string) {
  errStrings.push(string);
};

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

global.process.stdout.write = stdout_write;
global.process.stderr.write = stderr_write;

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
  assert.strictEqual(expected + '\n', strings.shift());
  assert.strictEqual(expected + '\n', errStrings.shift());
}

for (const expected of expectedStrings) {
  assert.strictEqual(expected + '\n', strings.shift());
  assert.strictEqual(expected + '\n', errStrings.shift());
}

assert.strictEqual("{ foo: 'bar', inspect: [Function: inspect] }\n",
                   strings.shift());
assert.strictEqual("{ foo: 'bar', inspect: [Function: inspect] }\n",
                   strings.shift());
assert.notEqual(-1, strings.shift().indexOf('foo: [Object]'));
assert.strictEqual(-1, strings.shift().indexOf('baz'));
assert.ok(/^label: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^__proto__: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^constructor: \d+\.\d{3}ms$/.test(strings.shift().trim()));
assert.ok(/^hasOwnProperty: \d+\.\d{3}ms$/.test(strings.shift().trim()));

assert.strictEqual('Trace: This is a {"formatted":"trace"} 10 foo',
                   errStrings.shift().split('\n').shift());

assert.strictEqual(strings.length, 0);
assert.strictEqual(errStrings.length, 0);

assert.throws(() => {
  console.assert(false, 'should throw');
}, /should throw/);

assert.doesNotThrow(() => {
  console.assert(true, 'this should not throw');
});
