'use strict';

// This test verifies that stack overflow during deeply nested async operations
// with async_hooks enabled can be caught by try-catch. This simulates real-world
// scenarios like processing deeply nested JSON structures where each level
// creates async operations (e.g., database calls, API requests).

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  const { createHook } = require('async_hooks');

  // Enable async_hooks with all callbacks (simulates APM tools)
  createHook({
    init() {},
    before() {},
    after() {},
    destroy() {},
    promiseResolve() {},
  }).enable();

  // Simulate an async operation (like a database call or API request)
  async function fetchThing(id) {
    return { id, data: `data-${id}` };
  }

  // Recursively process deeply nested data structure
  // This will cause stack overflow when the nesting is deep enough
  function processData(data, depth = 0) {
    if (Array.isArray(data)) {
      for (const item of data) {
        // Create a promise to trigger async_hooks init callback
        fetchThing(depth);
        processData(item, depth + 1);
      }
    }
  }

  // Create deeply nested array structure iteratively (to avoid stack overflow
  // during creation)
  function createNestedArray(depth) {
    let result = 'leaf';
    for (let i = 0; i < depth; i++) {
      result = [result];
    }
    return result;
  }

  // Create a very deep nesting that will cause stack overflow during processing
  const deeplyNested = createNestedArray(50000);

  try {
    processData(deeplyNested);
    // Should not complete successfully - the nesting is too deep
    console.log('UNEXPECTED: Processing completed without error');
    process.exit(1);
  } catch (err) {
    assert.strictEqual(err.name, 'RangeError');
    assert.match(err.message, /Maximum call stack size exceeded/);
    console.log('SUCCESS: try-catch caught the stack overflow in nested async');
    process.exit(0);
  }
} else {
  // Parent process - spawn the child and check exit code
  const result = spawnSync(
    process.execPath,
    [__filename, 'child'],
    { encoding: 'utf8', timeout: 30000 }
  );

  // Should exit successfully (try-catch worked)
  assert.strictEqual(result.status, 0,
                     `Expected exit code 0, got ${result.status}.\n` +
                     `stdout: ${result.stdout}\n` +
                     `stderr: ${result.stderr}`);
  // Verify the error was handled by try-catch
  assert.match(result.stdout, /SUCCESS: try-catch caught the stack overflow/);
}
