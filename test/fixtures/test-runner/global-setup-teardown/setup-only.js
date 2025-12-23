'use strict';

const fs = require('node:fs');

const setupFlagPath = process.env.SETUP_ONLY_FLAG_PATH;

async function globalSetup() {
  console.log('Setup-only module executed');
  fs.writeFileSync(setupFlagPath, 'Setup-only was executed');
}

module.exports = { globalSetup };
