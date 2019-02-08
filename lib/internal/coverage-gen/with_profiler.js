'use strict';

// Implements coverage collection exposed by the `NODE_V8_COVERAGE`
// environment variable which can also be used in the user land.

let coverageDirectory;

function writeCoverage() {
  const { join } = require('path');
  const { mkdirSync, writeFileSync } = require('fs');
  const { threadId } = require('internal/worker');

  const filename = `coverage-${process.pid}-${Date.now()}-${threadId}.json`;
  try {
    mkdirSync(coverageDirectory, { recursive: true });
  } catch (err) {
    if (err.code !== 'EEXIST') {
      console.error(err);
      return;
    }
  }

  const target = join(coverageDirectory, filename);
  try {
    disableAllAsyncHooks();
    internalBinding('coverage').end((msg) => {
      const coverageInfo = JSON.parse(msg).result;
      if (coverageInfo) {
        writeFileSync(target, JSON.stringify(coverageInfo));
      }
    });
  } catch (err) {
    console.error(err);
  }
}

function disableAllAsyncHooks() {
  const { getHookArrays } = require('internal/async_hooks');
  const [hooks_array] = getHookArrays();
  hooks_array.forEach((hook) => { hook.disable(); });
}

function setCoverageDirectory(dir) {
  coverageDirectory = dir;
}

module.exports = {
  writeCoverage,
  setCoverageDirectory
};
