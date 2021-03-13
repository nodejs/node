/* eslint-disable node-core/required-modules,node-core/require-common-first */

'use strict';

const { runInThisContext } = require('vm');
const { parentPort, workerData } = require('worker_threads');

const { ResourceLoader } = require(workerData.wptRunner);
const resource = new ResourceLoader(workerData.wptPath);
let testsStarted = 0;
let testsEnded = 0;

global.self = global;
global.GLOBAL = {
  isWindow() { return false; }
};
global.require = require;

// This is a mock, because at the moment fetch is not implemented
// in Node.js, but some tests and harness depend on this to pull
// resources.
global.fetch = function fetch(file) {
  return resource.read(workerData.testRelativePath, file, true);
};

if (workerData.initScript) {
  runInThisContext(workerData.initScript);
}

runInThisContext(workerData.harness.code, {
  filename: workerData.harness.filename
});

// eslint-disable-next-line no-undef
add_test_state_callback((changed, all) => {
  testsStarted = all.tests.length;
});

// eslint-disable-next-line no-undef
add_result_callback((result) => {
  testsEnded++;
  parentPort.postMessage({
    type: 'result',
    result: {
      status: result.status,
      name: result.name,
      message: result.message,
      stack: result.stack,
    },
  });
});

// eslint-disable-next-line no-undef
add_completion_callback((_, status) => {
  parentPort.postMessage({
    type: 'completion',
    status,
  });
});

process.on('uncaughtException', (err) => {
  if (testsStarted === 0 || testsStarted !== testsEnded) {
    throw err;
  }
});

for (const scriptToRun of workerData.scriptsToRun) {
  runInThisContext(scriptToRun.code, { filename: scriptToRun.filename });
}
