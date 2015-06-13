/* eslint-disable max-len */
'use strict';
var common = require('../common');
var assert = require('assert');

var content = require(common.fixturesDir +
  '/json-with-directory-name-module/module-stub/one-trailing-slash/two/three.js');

assert.notEqual(content.rocko, 'artischocko');
assert.equal(content, 'hello from module-stub!');
