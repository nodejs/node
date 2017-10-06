'use strict';
// Flags: --expose-internals

// This tests ensures that the type checking of ModuleMap throws errors appropriately

const assert = require('assert');

const ModuleMap = require('internal/loader/ModuleMap');

const moduleMap = new ModuleMap();

assert.throws(() => {
  moduleMap.get(1);
});
