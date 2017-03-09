'use strict';
require('../common');
const assert = require('assert');

(function testInjectFakeModule() {
  const relativePath = '../fixtures/semicolon';
  const absolutePath = require.resolve(relativePath);
  const fakeModule = {};

  require.cache[absolutePath] = {exports: fakeModule};

  assert.strictEqual(require(relativePath), fakeModule);
})();


(function testInjectFakeNativeModule() {
  const relativePath = 'fs';
  const fakeModule = {};

  require.cache[relativePath] = {exports: fakeModule};

  assert.strictEqual(require(relativePath), fakeModule);
})();
