'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/4_object_factory`);

const obj1 = addon('hello');
const obj2 = addon('world');
assert.strictEqual(`${obj1.msg} ${obj2.msg}`, 'hello world');
