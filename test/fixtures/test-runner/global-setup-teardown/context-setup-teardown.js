'use strict';

const fs = require('node:fs');

const contextFlagPath = process.env.CONTEXT_FLAG_PATH;

async function startServer() {
  return {
    async close() {
      fs.writeFileSync(contextFlagPath, 'server closed');
    },
  };
}

async function globalSetup(context) {
  context.server = await startServer();
}

async function globalTeardown(context) {
  await context.server.close();
}

module.exports = { globalSetup, globalTeardown };
