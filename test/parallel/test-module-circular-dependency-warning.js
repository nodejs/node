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
     'of module exports inside circular dependency'],
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

// If module.exports.__esModule is being accessed but is not present, e.g.
// because only the one of the files is a transpiled ES module, no warning
// should be emitted.
const halfTranspiledExport =
  require(fixtures.path('cycles', 'warning-esm-half-transpiled-a.js'));
assert.strictEqual(halfTranspiledExport.__esModule, undefined);

// No circular check is done to prevent triggering proxy traps, if
// module.exports is set to a proxy that contains a `getPrototypeOf` or
// `setPrototypeOf` trap.
require(fixtures.path('cycles', 'warning-skip-proxy-traps-a.js'));
