'use strict';

const fs = require('fs');

// Path for temporary file to track execution
const setupFlagPath = process.env.SETUP_FLAG_PATH;
const teardownFlagPath = process.env.TEARDOWN_FLAG_PATH;

async function globalSetup({ context }) {
  console.log('Global setup executed');
  context.setupData = 'data from setup';
  fs.writeFileSync(setupFlagPath, 'Setup was executed');
}

async function globalTeardown({ context }) {
  console.log('Global teardown executed');
  console.log(`Data from setup: ${context.setupData}`);
  fs.writeFileSync(teardownFlagPath, 'Teardown was executed');

  // Clean up the setup file too
  if (fs.existsSync(setupFlagPath)) {
    fs.unlinkSync(setupFlagPath);
  }
}

module.exports = { globalSetup, globalTeardown };
