'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

Object.defineProperty(Object.prototype, 'name', {
  __proto__: null,
  get: common.mustNotCall('get %Object.prototype%.name'),
  set: common.mustNotCall('set %Object.prototype%.name'),
  enumerable: false,
});
Object.defineProperty(Object.prototype, 'main', {
  __proto__: null,
  get: common.mustNotCall('get %Object.prototype%.main'),
  set: common.mustNotCall('set %Object.prototype%.main'),
  enumerable: false,
});
Object.defineProperty(Object.prototype, 'type', {
  __proto__: null,
  get: common.mustNotCall('get %Object.prototype%.type'),
  set: common.mustNotCall('set %Object.prototype%.type'),
  enumerable: false,
});
Object.defineProperty(Object.prototype, 'exports', {
  __proto__: null,
  get: common.mustNotCall('get %Object.prototype%.exports'),
  set: common.mustNotCall('set %Object.prototype%.exports'),
  enumerable: false,
});
Object.defineProperty(Object.prototype, 'imports', {
  __proto__: null,
  get: common.mustNotCall('get %Object.prototype%.imports'),
  set: common.mustNotCall('set %Object.prototype%.imports'),
  enumerable: false,
});

assert.strictEqual(
  require(fixtures.path('es-module-specifiers', 'node_modules', 'no-main-field')),
  'no main field'
);

import(fixtures.fileURL('es-module-specifiers', 'index.mjs'))
  .then(common.mustCall((module) => assert.strictEqual(module.noMain, 'no main field')));
