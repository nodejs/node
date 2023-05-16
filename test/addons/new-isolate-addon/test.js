'use strict';
const common = require('../../common');
const binding = require(`./build/${common.buildType}/binding`);
const assert = require('assert');

const arg = new SharedArrayBuffer(1);
binding.runInSeparateIsolate('const arr = new Uint8Array(arg); arr[0] = 0x42;', arg);
assert.deepStrictEqual(new Uint8Array(arg), new Uint8Array([0x42]));
