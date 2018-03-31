'use strict';
const common = require('../../common');
const assert = require('assert');

const re = /^Error: Module did not self-register or module required and registered with different names\.$/;
assert.throws(() => require(`./build/${common.buildType}/binding`), re);
