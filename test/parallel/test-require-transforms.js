'use strict';

const common = require('../common');
const assert = require('assert');

// Juts so lint doesn't complain about unused common (which is required)
common.refreshTmpDir();

function fooTransform(content, filename) {
  if (!/test-require-transforms\.js$/.test(filename)) {
    return content;
  }
  return 'module.exports = "foo"';
}
function barTransform(content, filename) {
  if (!/test-require-transforms\.js$/.test(filename)) {
    return content;
  }
  return content.replace(/foo/, 'bar');
}

// Test one-step transform
require.addTransform(fooTransform);
delete require.cache[require.resolve('./test-require-transforms')];
const oneStep = require('./test-require-transforms');
assert.strictEqual(oneStep, 'foo');

// Test two-step transform
require.addTransform(barTransform);
delete require.cache[require.resolve('./test-require-transforms')];
const twoStep = require('./test-require-transforms');
assert.strictEqual(twoStep, 'bar');

// Test transform step removal
require.removeTransform(barTransform);
delete require.cache[require.resolve('./test-require-transforms')];
const removal = require('./test-require-transforms');
assert.strictEqual(removal, 'foo');
