'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const content = require(fixtures.path('json-with-directory-name-module',
                                      'module-stub', 'one', 'two',
                                      'three.js'));

assert.notStrictEqual(content.rocko, 'artischocko');
assert.strictEqual(content, 'hello from module-stub!');
