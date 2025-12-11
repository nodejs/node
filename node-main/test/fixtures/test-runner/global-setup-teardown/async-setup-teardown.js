'use strict';

const fs = require('node:fs');
const { setTimeout } = require('node:timers/promises');

const asyncFlagPath = process.env.ASYNC_FLAG_PATH;

async function globalSetup() {
  console.log('Async setup starting');

  await setTimeout(500);

  fs.writeFileSync(asyncFlagPath, 'Setup part');
  console.log('Async setup completed');
}

async function globalTeardown() {
  console.log('Async teardown starting');

  await setTimeout(100);

  fs.appendFileSync(asyncFlagPath, ', Teardown part');
  console.log('Async teardown completed');
}

module.exports = { globalSetup, globalTeardown };
