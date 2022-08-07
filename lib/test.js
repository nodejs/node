'use strict';
const { after, afterEach, before, beforeEach, describe, it, test } = require('internal/test_runner/harness');
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
