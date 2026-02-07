'use strict';

const fs = require('node:fs');

const contextFlagPath = process.env.CONTEXT_FLAG_PATH;

async function globalSetup() {
  return {
    value: 'context from setup',
  };
}

async function globalTeardown(context) {
  fs.writeFileSync(contextFlagPath, context?.value ?? 'missing context');
}

module.exports = { globalSetup, globalTeardown };
