'use strict';
// Flags: --no-deprecated-process-binding
require('../common');
const { strictEqual } = require('node:assert');
strictEqual(process.binding, undefined);
