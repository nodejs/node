'use strict';

require('../common');
const assert = require('assert');

const isLinux = process.platform === 'linux';

const O_NOATIME = process.binding('constants').O_NOATIME;
const expected = isLinux ? 0x40000 : undefined;

assert.strictEqual(O_NOATIME, expected);
