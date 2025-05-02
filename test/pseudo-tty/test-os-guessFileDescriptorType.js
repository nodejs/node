'use strict';

require('../common');
const { strictEqual } = require('assert');
const { guessFileDescriptorType } = require('os');

strictEqual(guessFileDescriptorType(0), 'TTY', 'stdin reported to not be a tty, but it is');
strictEqual(guessFileDescriptorType(1), 'TTY', 'stdout reported to not be a tty, but it is');
strictEqual(guessFileDescriptorType(2), 'TTY', 'stderr reported to not be a tty, but it is');

strictEqual(guessFileDescriptorType(-1), 'INVALID', '-1 reported to be a tty, but it is not');
strictEqual(guessFileDescriptorType(55555), 'UNKNOWN', '55555 reported to be a tty, but it is not');
strictEqual(guessFileDescriptorType(2 ** 31), 'INVALID', '2^31 reported to be a tty, but it is not');
strictEqual(guessFileDescriptorType(1.1), 'INVALID', '1.1 reported to be a tty, but it is not');
strictEqual(guessFileDescriptorType('1'), 'INVALID', '\'1\' reported to be a tty, but it is not');
strictEqual(guessFileDescriptorType({}), 'INVALID', '{} reported to be a tty, but it is not');
strictEqual(guessFileDescriptorType(() => {}), 'INVALID', '() => {} reported to be a tty, but it is not');
