'use strict';
var common = require('../common');
var assert = require('assert');

(function testInjectFakeModule() {
  var relativePath = '../fixtures/semicolon';
  var absolutePath = require.resolve(relativePath);
  var fakeModule = {};

  require.cache[absolutePath] = {exports: fakeModule};

  assert.strictEqual(require(relativePath), fakeModule);
})();


(function testInjectFakeNativeModule() {
  var relativePath = 'fs';
  var fakeModule = {};

  require.cache[relativePath] = {exports: fakeModule};

  assert.strictEqual(require(relativePath), fakeModule);
})();
