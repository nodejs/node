'use strict';
const common = require('../../common');
const assert = require('assert');
assert.throws(() => require(`./build/${common.buildType}/binding`), /node_abi_icu_version/);
