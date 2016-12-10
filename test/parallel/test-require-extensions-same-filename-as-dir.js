'use strict';
const common = require('../common');
const assert = require('assert');

const content = require(common.fixturesDir +
  '/json-with-directory-name-module/module-stub/one/two/three.js');

assert.notStrictEqual(content.rocko, 'artischocko');
assert.strictEqual(content, 'hello from module-stub!');
