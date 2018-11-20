'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/binding`);

const fn = addon();
assert.strictEqual(fn(), 'hello world'); // 'hello world'
