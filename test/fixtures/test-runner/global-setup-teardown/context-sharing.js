'use strict';

const fs = require('fs');

const contextFlagPath = process.env.CONTEXT_FLAG_PATH;

async function globalSetup({ context }) {
  // Store data in the context to be used by teardown
  context.timestamp = Date.now();
  context.message = 'Hello from setup';
  context.complexData = { key: 'value', nested: { data: true } };
}

async function globalTeardown({ context }) {
  // Write the context data to a file to verify it was preserved
  const contextData = {
    timestamp: context.timestamp,
    message: context.message,
    complexData: context.complexData,
  };

  fs.writeFileSync(contextFlagPath, JSON.stringify(contextData));
}

module.exports = { globalSetup, globalTeardown };
