'use strict';
require('../common');
const { Console } = require('console');
const { PassThrough } = require('stream');
const { strict: assert } = require('assert');

const stdout = new PassThrough().setEncoding('utf8');
const stderr = new PassThrough().setEncoding('utf8');

const console = new Console({
  stdout,
  stderr,
  inspectOptions: new Map([
    [stdout, { colors: true }],
    [stderr, { colors: false }],
  ]),
});

console.log('Hello', 42);
console.warn('Hello', 42);

assert.strictEqual(stdout.read(), 'Hello \x1B[33m42\x1B[39m\n');
assert.strictEqual(stderr.read(), 'Hello 42\n');
