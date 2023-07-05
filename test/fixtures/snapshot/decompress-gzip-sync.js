'use strict';

const zlib = require('zlib');
const fs = require('fs');
const assert = require('assert');

const fixture = process.env.NODE_TEST_FIXTURE;
const mode = process.env.NODE_TEST_MODE;
const file = fs.readFileSync(fixture);
const result = zlib.gunzipSync(file);

console.log(`Result length = ${result.byteLength}`);
console.log('NODE_TEST_MODE:', mode);
if (mode === 'snapshot') {
  globalThis.NODE_TEST_DATA = result;
} else if (mode === 'verify') {
  assert.deepStrictEqual(globalThis.NODE_TEST_DATA, result);
} else {
  assert.fail('Unknown mode');
}
