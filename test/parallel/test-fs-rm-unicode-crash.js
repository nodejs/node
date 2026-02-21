'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

if (process.platform !== 'win32') {
  common.skip('This test only runs on Windows');
}

const filename = path.join(__dirname, '速');

fs.writeFileSync(filename, 'hello');

try {
  fs.rmSync(filename);
  assert(!fs.existsSync(filename), 'File should be deleted');
} catch (err) {
  assert.fail(`Unexpected error: ${err.message}`);
}
