'use strict';
require('../common');
const assert = require('assert');

// check for existence
assert(Object.hasOwn(process.config.variables, 'node_module_version'));

// Ensure that `node_module_version` is an Integer > 0
assert(Number.isInteger(process.config.variables.node_module_version));
assert(process.config.variables.node_module_version > 0);
