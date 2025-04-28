'use strict';

require('../common');
const { strictEqual } = require('assert');
const { guessHandleType } = require('os');

strictEqual(guessHandleType(0), 'TTY', 'stdin reported to not be a tty, but it is');
strictEqual(guessHandleType(1), 'TTY', 'stdout reported to not be a tty, but it is');
strictEqual(guessHandleType(2), 'TTY', 'stderr reported to not be a tty, but it is');

strictEqual(guessHandleType(-1), 'INVALID', '-1 reported to be a tty, but it is not');
strictEqual(guessHandleType(55555), 'UNKNOWN', '55555 reported to be a tty, but it is not');
strictEqual(guessHandleType(2 ** 31), 'INVALID', '2^31 reported to be a tty, but it is not');
strictEqual(guessHandleType(1.1), 'INVALID', '1.1 reported to be a tty, but it is not');
strictEqual(guessHandleType('1'), 'INVALID', '\'1\' reported to be a tty, but it is not');
