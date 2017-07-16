/* eslint-disable max-len */
'use strict';
const common = require('../common');
const assert = require('assert');
const filePath = '/json-with-directory-name-module/module-stub/one-trailing-slash/two/three.js';
const content = require(`${common.fixturesDir}${filePath}`);

assert.notStrictEqual(content.rocko, 'artischocko');
assert.strictEqual(content, 'hello from module-stub!');
