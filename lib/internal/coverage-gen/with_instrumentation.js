'use strict';

// This file contains hooks for nyc instrumented lib/ files to collect
// JS coverage for core.
// See `make coverage-build`.
function writeCoverage() {
  if (!global.__coverage__) {
    return;
  }

  const path = require('path');
  const { mkdirSync, writeFileSync } = require('fs');

  const dirname = path.join(path.dirname(process.execPath), '.coverage');
  const filename = `coverage-${process.pid}-${Date.now()}.json`;
  try {
    mkdirSync(dirname);
  } catch (err) {
    if (err.code !== 'EEXIST') {
      console.error(err);
      return;
    }
  }

  const target = path.join(dirname, filename);
  const coverageInfo = JSON.stringify(global.__coverage__);
  try {
    writeFileSync(target, coverageInfo);
  } catch (err) {
    console.error(err);
  }
}

module.exports = {
  writeCoverage
};
