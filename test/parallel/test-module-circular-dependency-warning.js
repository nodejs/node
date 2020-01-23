'use strict';

const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

common.expectWarning({
  Warning: [
    ["Accessing non-existent property 'missingPropB' " +
     'of module exports inside circular dependency'],
    ["Accessing non-existent property 'Symbol(someSymbol)' " +
     'of module exports inside circular dependency'],
    ["Accessing non-existent property 'missingPropModuleExportsB' " +
     'of module exports inside circular dependency']
  ]
});
const required = require(fixtures.path('cycles', 'warning-a.js'));
assert.strictEqual(Object.getPrototypeOf(required), Object.prototype);

const requiredWithModuleExportsOverridden =
  require(fixtures.path('cycles', 'warning-moduleexports-a.js'));
assert.strictEqual(Object.getPrototypeOf(requiredWithModuleExportsOverridden),
                   Object.prototype);

// If module.exports is not a regular object, no warning should be emitted.
const classExport =
  require(fixtures.path('cycles', 'warning-moduleexports-class-a.js'));
assert.strictEqual(Object.getPrototypeOf(classExport).name, 'Parent');

// If module.exports.__esModule is set, no warning should be emitted.
const esmTranspiledExport =
  require(fixtures.path('cycles', 'warning-esm-transpiled-a.js'));
assert.strictEqual(esmTranspiledExport.__esModule, true);
