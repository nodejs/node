'use strict';

require('../common');
const { strictEqual } = require('assert');
const { isatty } = require('tty');

strictEqual(isatty(0), true, 'stdin reported to not be a tty, but it is');
strictEqual(isatty(1), true, 'stdout reported to not be a tty, but it is');
strictEqual(isatty(2), true, 'stderr reported to not be a tty, but it is');

strictEqual(isatty(-1), false, '-1 reported to be a tty, but it is not');
strictEqual(isatty(55555), false, '55555 reported to be a tty, but it is not');
strictEqual(isatty(2 ** 31), false, '2^31 reported to be a tty, but it is not');
strictEqual(isatty(1.1), false, '1.1 reported to be a tty, but it is not');
strictEqual(isatty('1'), false, '\'1\' reported to be a tty, but it is not');
strictEqual(isatty({}), false, '{} reported to be a tty, but it is not');
strictEqual(isatty(() => {}), false,
            '() => {} reported to be a tty, but it is not');
