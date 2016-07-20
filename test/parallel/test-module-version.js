'use strict';
require('../common');
const assert = require('assert');

// Check for existence
assert(process.config.variables.hasOwnProperty('node_module_version'));

// Ensure that `node_module_version` is an Integer
assert(!Number.isNaN(parseInt(process.config.variables.node_module_version)));
assert.strictEqual(process.config.variables.node_module_version, 46);
