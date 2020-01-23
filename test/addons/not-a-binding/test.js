'use strict';
const common = require('../../common');
const assert = require('assert');

const re = /^Error: Module did not self-register: '.*[\\/]binding\.node'\.$/;
assert.throws(() => require(`./build/${common.buildType}/binding`), re);
