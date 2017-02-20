'use strict';

require('../common');
const assert = require('assert');
const util = require('util');

const inspectTag = util.inspectTag;

const obj = {a: 1};

assert.strictEqual(`${obj}`, '[object Object]');
assert.strictEqual(inspectTag`${obj}`, '{ a: 1 }');
assert.strictEqual(inspectTag`${obj}-${obj}`, '{ a: 1 }-{ a: 1 }');
