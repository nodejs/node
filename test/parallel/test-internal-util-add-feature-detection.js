// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { addFeatureDetection } = require('internal/util');
const { features } = require('util');

function myFunction() {
  return true;
}

addFeatureDetection(myFunction, {
  works: true,
});

assert.strictEqual(typeof myFunction, 'function');
assert.strictEqual(myFunction(), true);
assert.strictEqual(myFunction[features].works, true);
assert.strictEqual(myFunction[features].doesNotWork, undefined);
