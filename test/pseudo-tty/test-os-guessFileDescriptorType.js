'use strict';

require('../common');
const { strictEqual, throws } = require('assert');
const { guessFileDescriptorType } = require('os');

strictEqual(guessFileDescriptorType(0), 'TTY', 'stdin reported to not be a tty, but it is');
strictEqual(guessFileDescriptorType(1), 'TTY', 'stdout reported to not be a tty, but it is');
strictEqual(guessFileDescriptorType(2), 'TTY', 'stderr reported to not be a tty, but it is');

strictEqual(guessFileDescriptorType(55555), 'UNKNOWN', '55555 reported to be a handle, but it is not');
strictEqual(guessFileDescriptorType(2 ** 31 - 1), 'UNKNOWN', '2^31-1 reported to be a handle, but it is not');

throws(() => guessFileDescriptorType(-1), /"fd" must be a positive integer/, '-1 reported to be a handle, but it is not');
throws(() => guessFileDescriptorType(1.1), /"fd" must be a positive integer/, '1.1 reported to be a handle, but it is not');
throws(() => guessFileDescriptorType('1'), /"fd" must be a positive integer/, '\'1\' reported to be a tty, but it is not');
throws(() => guessFileDescriptorType({}), /"fd" must be a positive integer/, '{} reported to be a tty, but it is not');
throws(() => guessFileDescriptorType(() => {}), /"fd" must be a positive integer/, '() => {} reported to be a tty, but it is not');
throws(() => guessFileDescriptorType(2 ** 31), /"fd" must be a positive integer/, '2^31 reported to be a handle, but it is not (because the fd check rolls over the input to negative of it)');
