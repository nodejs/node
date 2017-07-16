'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');

const filePath = path.join(
  common.fixturesDir,
  'json-with-directory-name-module',
  'module-stub',
  'one',
  'two',
  'three.js'
);
const content = require(filePath);

assert.notStrictEqual(content.rocko, 'artischocko');
assert.strictEqual(content, 'hello from module-stub!');
