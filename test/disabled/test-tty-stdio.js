'use strict';
// Can't test this when 'make test' doesn't assign a tty to the stdout.
var common = require('../common');
var assert = require('assert');
var tty = require('tty');

assert.ok(process.stdin instanceof tty.ReadStream);
assert.ok(process.stdin.readable);
assert.ok(!process.stdin.writable);

assert.ok(process.stdout instanceof tty.WriteStream);
assert.ok(!process.stdout.readable);
assert.ok(process.stdout.writable);
