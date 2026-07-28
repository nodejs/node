'use strict';

const fs = require('node:fs');

// Path for temporary file to track execution
const setupFlagPath = process.env.SETUP_FLAG_PATH;
const teardownFlagPath = process.env.TEARDOWN_FLAG_PATH;

async function globalSetup() {
  console.log('Global setup executed');
  fs.writeFileSync(setupFlagPath, 'Setup was executed');
}

async function globalTeardown() {
  console.log('Global teardown executed');
  fs.writeFileSync(teardownFlagPath, 'Teardown was executed');
  fs.rmSync(setupFlagPath, { force: true });
}

module.exports = { globalSetup, globalTeardown };
