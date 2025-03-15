'use strict';

const fs = require('fs');

const asyncFlagPath = process.env.ASYNC_FLAG_PATH;

async function globalSetup() {
  console.log('Async setup starting');

  // Simulate an async operation
  await new Promise((resolve) => setTimeout(resolve, 100));

  fs.writeFileSync(asyncFlagPath, 'Setup part');
  console.log('Async setup completed');
}

async function globalTeardown() {
  console.log('Async teardown starting');

  // Simulate an async operation
  await new Promise((resolve) => setTimeout(resolve, 100));

  // Append to the file
  fs.appendFileSync(asyncFlagPath, ', Teardown part');
  console.log('Async teardown completed');
}

module.exports = { globalSetup, globalTeardown };
