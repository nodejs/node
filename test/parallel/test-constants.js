'use strict';

require('../common');
const constants = process.binding('constants');
const assert = require('assert');

assert.ok(constants);
assert.ok(constants.os);
assert.ok(constants.os.signals);
assert.ok(constants.os.errno);
assert.ok(constants.fs);
assert.ok(constants.crypto);
