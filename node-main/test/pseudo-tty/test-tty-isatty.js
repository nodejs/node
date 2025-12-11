'use strict';

require('../common');
const assert = require('assert');
const { isatty } = require('tty');

assert.ok(isatty(0), 'stdin reported to not be a tty, but it is');
assert.ok(isatty(1), 'stdout reported to not be a tty, but it is');
assert.ok(isatty(2), 'stderr reported to not be a tty, but it is');

assert.ok(!isatty(-1), '-1 reported to be a tty, but it is not');
assert.ok(!isatty(55555), '55555 reported to be a tty, but it is not');
assert.ok(!isatty(2 ** 31), '2^31 reported to be a tty, but it is not');
assert.ok(!isatty(1.1), '1.1 reported to be a tty, but it is not');
assert.ok(!isatty('1'), '\'1\' reported to be a tty, but it is not');
assert.ok(!isatty({}), '{} reported to be a tty, but it is not');
assert.ok(!isatty(() => {}), '() => {} reported to be a tty, but it is not');
