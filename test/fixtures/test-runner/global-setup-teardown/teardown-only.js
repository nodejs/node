'use strict';

const fs = require('node:fs');

const teardownFlagPath = process.env.TEARDOWN_ONLY_FLAG_PATH;

async function globalTeardown() {
  console.log('Teardown-only module executed');
  fs.writeFileSync(teardownFlagPath, 'Teardown-only was executed');
}

module.exports = { globalTeardown };
