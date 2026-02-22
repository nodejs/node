'use strict';

// Test that ESM loader handles null/undefined source gracefully
// and throws meaningful error instead of ERR_INTERNAL_ASSERTION.
// Refs: https://github.com/nodejs/node/issues/60401

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

// Reusable loader functions
function createNullLoader() {
  return function load(url, context, next) {
    if (url.includes('test-null-source')) {
      return { format: 'commonjs', source: null, shortCircuit: true };
    }
    return next(url);
  };
}

function createUndefinedLoader() {
  return function load(url, context, next) {
    if (url.includes('test-undefined-source')) {
      return { format: 'commonjs', source: undefined, shortCircuit: true };
    }
    return next(url);
  };
}

function createEmptyLoader() {
  return function load(url, context, next) {
    if (url.includes('test-empty-source')) {
      return { format: 'commonjs', source: '', shortCircuit: true };
    }
    return next(url);
  };
}

// Helper to run test with custom loader
function runTestWithLoader(loaderFn, testUrl) {
  const loaderCode = `export ${loaderFn.toString()}`;
  
  const result = spawnSync(
    process.execPath,
    [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `
      import { register } from 'node:module';
      import assert from 'node:assert';
      
      register('data:text/javascript,' + encodeURIComponent(${JSON.stringify(loaderCode)}));
      
      await assert.rejects(
        import(${JSON.stringify(testUrl)}),
        { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' }
      );
      `,
    ],
    { encoding: 'utf8' }
  );

  return result;
}

// Test case 1: Loader returning null source
{
  const result = runTestWithLoader(
    createNullLoader(),
    'file:///test-null-source.js'
  );

  const output = result.stdout + result.stderr;
  
  assert.ok(
    !output.includes('ERR_INTERNAL_ASSERTION'),
    'Should not throw ERR_INTERNAL_ASSERTION. Output: ' + output
  );

  assert.strictEqual(
    result.status,
    0,
    'Process should exit with code 0. Output: ' + output
  );
}

// Test case 2: Loader returning undefined source
{
  const result = runTestWithLoader(
    createUndefinedLoader(),
    'file:///test-undefined-source.js'
  );

  const output = result.stdout + result.stderr;
  
  assert.ok(
    !output.includes('ERR_INTERNAL_ASSERTION'),
    'Should not throw ERR_INTERNAL_ASSERTION. Output: ' + output
  );

  assert.strictEqual(
    result.status,
    0,
    'Process should exit with code 0. Output: ' + output
  );
}

// Test case 3: Loader returning empty string source
{
  const fixtureURL = fixtures.fileURL('es-modules/loose.js');
  
  const result = runTestWithLoader(
    createEmptyLoader(),
    fixtureURL.href
  );

  const output = result.stdout + result.stderr;
  
  assert.ok(
    !output.includes('ERR_INTERNAL_ASSERTION'),
    'Should not throw ERR_INTERNAL_ASSERTION. Output: ' + output
  );

  assert.strictEqual(
    result.status,
    0,
    'Process should exit with code 0. Output: ' + output
  );
}
