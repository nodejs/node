'use strict';
const test = require('internal/test_runner/harness');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('The test runner');

module.exports = test;
module.exports.test = test;
