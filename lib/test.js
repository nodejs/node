'use strict';
const { test, describe, it } = require('internal/test_runner/harness');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('The test runner');

module.exports = test;
module.exports.test = test;
module.exports.describe = describe;
module.exports.it = it;
