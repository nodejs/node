'use strict';

const common = require('../../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const bytes = new Uint8Array(1024);
assert(binding.randomBytes(bytes));
assert(bytes.reduce((v, a) => v + a) > 0);
assert(binding.hash(bytes));
