'use strict';
const { test, describe, it, before, after, beforeEach, afterEach } = require('internal/test_runner/harness');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('The test runner');

module.exports = test;
module.exports.test = test;
module.exports.describe = describe;
module.exports.it = it;
module.exports.before = before;
module.exports.after = after;
module.exports.beforeEach = beforeEach;
module.exports.afterEach = afterEach;
