'use strict';
require('../common');
const assert = require('assert');

process.stdin.setRawMode(true);
assert(process.stdin.isRaw);

process.stdin.setRawMode(false);
assert(!process.stdin.isRaw);
