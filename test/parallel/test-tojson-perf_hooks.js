'use strict';

require('../common');
const assert = require('assert');
const { performance } = require('perf_hooks');

// Test toJSON for performance object
{
  assert.strictEqual(typeof performance.toJSON, 'function');
  const jsonObject = performance.toJSON();
  assert.strictEqual(typeof jsonObject, 'object');
  assert.strictEqual(jsonObject.timeOrigin, performance.timeOrigin);
  assert.strictEqual(typeof jsonObject.nodeTiming, 'object');
}
