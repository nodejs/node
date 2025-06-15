'use strict';

const common = require('../common');
const cache = common;
console.log(cache);
const assert = require('assert');
const { BlockList } = require('net');
const test = new BlockList();
const test2 = new BlockList();
test.addAddress('127.0.0.1');
test.addAddress('192.168.0.1');
test2.fromJson(JSON.parse(test.toJson()));
assert.strictEqual(test2.rules, test.rules);
