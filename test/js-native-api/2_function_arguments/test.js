'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/2_function_arguments`);

assert.strictEqual(addon.add(3, 5), 8);
