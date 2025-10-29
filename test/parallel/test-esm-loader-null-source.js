'use strict';

// Test that ESM loader handles null/undefined source gracefully
// and throws meaningful error instead of ERR_INTERNAL_ASSERTION.
// Refs: https://github.com/nodejs/node/issues/60401

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

// Test case: Loader returning null source for CommonJS module
// This should throw ERR_INVALID_RETURN_PROPERTY_VALUE, not ERR_INTERNAL_ASSERTION
{
  const result = spawnSync(
    process.execPath,
    [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `
      import { register } from 'node:module';
      
      // Register a custom loader that returns null source
      const code = 'export function load(url, context, next) {' +
        '  if (url.includes("test-null-source")) {' +
        '    return { format: "commonjs", source: null, shortCircuit: true };' +
        '  }' +
        '  return next(url);' +
        '}';
      
      register('data:text/javascript,' + encodeURIComponent(code));
      
      await assert.rejects(import('file:///test-null-source.js'), { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' });
      `,
    ],
    { encoding: 'utf8' }
  );

  const output = result.stdout + result.stderr;
  
  // Verify test passed
  assert.ok(
    output.includes('PASS: Got expected error'),
    'Should pass with expected error. Output: ' + output
  );
  
  assert.strictEqual(
    result.status,
    0,
    'Process should exit with code 0. Output: ' + output
  );
}

// Test case: Loader returning undefined source
{
  const result = spawnSync(
    process.execPath,
    [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `
      import { register } from 'node:module';
      
      const code = 'export function load(url, context, next) {' +
        '  if (url.includes("test-undefined-source")) {' +
        '    return { format: "commonjs", source: undefined, shortCircuit: true };' +
        '  }' +
        '  return next(url);' +
        '}';
      
      register('data:text/javascript,' + encodeURIComponent(code));
      
      try {
        await import('file:///test-undefined-source.js');
        console.log('ERROR: Should have thrown');
        process.exit(1);
      } catch (err) {
        if (err.code === 'ERR_INTERNAL_ASSERTION') {
          console.log('FAIL: Got ERR_INTERNAL_ASSERTION');
          process.exit(1);
        }
        if (err.code === 'ERR_INVALID_RETURN_PROPERTY_VALUE') {
          console.log('PASS: Got expected error');
          process.exit(0);
        }
        console.log('ERROR: Got unexpected error:', err.code);
        process.exit(1);
      }
      `,
    ],
    { encoding: 'utf8' }
  );

  const output = result.stdout + result.stderr;
  
  assert.ok(
    output.includes('PASS: Got expected error'),
    'Should pass with expected error for undefined. Output: ' + output
  );
  
  assert.strictEqual(
    result.status,
    0,
    'Process should exit with code 0. Output: ' + output
  );
}

// Test case: Loader returning empty string source
{
  const result = spawnSync(
    process.execPath,
    [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `
      import { register } from 'node:module';
      
      const code = 'export function load(url, context, next) {' +
        '  if (url.includes("test-empty-source")) {' +
        '    return { format: "commonjs", source: "", shortCircuit: true };' +
        '  }' +
        '  return next(url);' +
        '}';
      
      register('data:text/javascript,' + encodeURIComponent(code));
      
      try {
        await import('file:///test-empty-source.js');
        console.log('ERROR: Should have thrown');
        process.exit(1);
      } catch (err) {
        if (err.code === 'ERR_INTERNAL_ASSERTION') {
          console.log('FAIL: Got ERR_INTERNAL_ASSERTION');
          process.exit(1);
        }
        if (err.code === 'ERR_INVALID_RETURN_PROPERTY_VALUE') {
          console.log('PASS: Got expected error');
          process.exit(0);
        }
        console.log('ERROR: Got unexpected error:', err.code);
        process.exit(1);
      }
      `,
    ],
    { encoding: 'utf8' }
  );

  const output = result.stdout + result.stderr;
  
  assert.ok(
    output.includes('PASS: Got expected error'),
    'Should pass with expected error for empty string. Output: ' + output
  );
  
  assert.strictEqual(
    result.status,
    0,
    'Process should exit with code 0. Output: ' + output
  );
}

console.log('All tests passed!');
