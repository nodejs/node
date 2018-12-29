'use strict';

// Implements coverage collection exposed by the `NODE_V8_COVERAGE`
// environment variable which can also be used in the user land.

let coverageConnection = null;
let coverageDirectory;

function writeCoverage() {
  if (!coverageConnection && coverageDirectory) {
    return;
  }

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
    let msg;
    coverageConnection._coverageCallback = function(_msg) {
      msg = _msg;
    };
    coverageConnection.dispatch(JSON.stringify({
      id: 3,
      method: 'Profiler.takePreciseCoverage'
    }));
    const coverageInfo = JSON.parse(msg).result;
    writeFileSync(target, JSON.stringify(coverageInfo));
  } catch (err) {
    console.error(err);
  } finally {
    coverageConnection.disconnect();
    coverageConnection = null;
  }
}

function disableAllAsyncHooks() {
  const { getHookArrays } = require('internal/async_hooks');
  const [hooks_array] = getHookArrays();
  hooks_array.forEach((hook) => { hook.disable(); });
}

function startCoverageCollection() {
  const { Connection } = internalBinding('inspector');
  coverageConnection = new Connection((res) => {
    if (coverageConnection._coverageCallback) {
      coverageConnection._coverageCallback(res);
    }
  });
  coverageConnection.dispatch(JSON.stringify({
    id: 1,
    method: 'Profiler.enable'
  }));
  coverageConnection.dispatch(JSON.stringify({
    id: 2,
    method: 'Profiler.startPreciseCoverage',
    params: {
      callCount: true,
      detailed: true
    }
  }));
}

function setCoverageDirectory(dir) {
  coverageDirectory = dir;
}

module.exports = {
  startCoverageCollection,
  writeCoverage,
  setCoverageDirectory
};
