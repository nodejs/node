'use strict';

const fs = require('fs');

const teardownFlagPath = process.env.TEARDOWN_ONLY_FLAG_PATH;

async function globalTeardown({ reporter }) {
  console.log('Teardown-only module executed');
  fs.writeFileSync(teardownFlagPath, 'Teardown-only was executed');
}

module.exports = { globalTeardown };
