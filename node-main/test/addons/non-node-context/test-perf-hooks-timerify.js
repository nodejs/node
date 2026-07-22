'use strict';

const common = require('../../common');
const assert = require('assert');
const { runInNewContext } = require(`./build/${common.buildType}/binding`);
const { performance } = require('perf_hooks');

// Check that performance.timerify() works when called from another context,
// for a function created in another context.

const check = runInNewContext(`
const { performance, assert } = data;
const timerified = performance.timerify(function() { return []; });
assert.strictEqual(timerified().constructor, Array);
'success';
`, { performance, assert });
assert.strictEqual(check, 'success');
