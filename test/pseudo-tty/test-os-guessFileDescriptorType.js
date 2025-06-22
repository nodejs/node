'use strict';

require('../common');
const { strictEqual, throws } = require('assert');
const { guessFileDescriptorType } = require('os');

strictEqual(guessFileDescriptorType(0), 'TTY', 'stdin reported to not be a tty, but it is');
strictEqual(guessFileDescriptorType(1), 'TTY', 'stdout reported to not be a tty, but it is');
strictEqual(guessFileDescriptorType(2), 'TTY', 'stderr reported to not be a tty, but it is');

strictEqual(guessFileDescriptorType(55555), 'UNKNOWN', '55555 reported to be a handle, but it is not');
strictEqual(guessFileDescriptorType(2 ** 31 - 1), 'UNKNOWN', '2^31-1 reported to be a handle, but it is not');

[
    -1,
    1.1,
    '1',
    [],
    {},
    () => {},
    2 ** 31,
    true,
    false,
    1n,
    Symbol(),
    undefined,
    null,
].forEach((val) => throws(() => guessFileDescriptorType(val), { code: 'ERR_INVALID_FD' }));
