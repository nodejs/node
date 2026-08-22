'use strict';

const { execSync } = require('child_process');
const assert = require('assert');

const nodeBinary = process.execPath;

try {
  const output = execSync(
    `${nodeBinary} -p "await Promise.resolve(123)" --input-type=module`
  ).toString().trim();

  assert.strictEqual(output, '123');
  console.log('Test passed: ESM --print works with top-level await');
} catch (err) {
  console.error('❌ Test failed:', err.stderr?.toString() || err.message);
  throw err;
}
