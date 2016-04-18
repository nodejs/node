'use strict';

require('../../common');
const assert = require('assert');
const binding = require('./build/Release/binding');
const bytes = new Uint8Array(1024);
assert(binding.randomBytes(bytes));
assert(bytes.reduce((v, a) => v + a) > 0);
