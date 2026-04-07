'use strict';

const {
  runInNewContext,
  runInThisContext,
  constants: { USE_MAIN_CONTEXT_DEFAULT_LOADER },
} = require('vm');
const { setFlagsFromString } = require('v8');
const { parentPort, workerData } = require('worker_threads');

const { ResourceLoader } = require(workerData.wptRunner);
const resource = new ResourceLoader(workerData.wptPath);

if (workerData.needsGc) {
  // See https://github.com/nodejs/node/issues/16595#issuecomment-340288680
  setFlagsFromString('--expose-gc');
  globalThis.gc = runInNewContext('gc');
}

globalThis.self = global;
globalThis.GLOBAL = {
  isWindow() { return false; },
  isShadowRealm() { return false; },
};
globalThis.require = require;

// This is a mock for non-fetch tests that use fetch to resolve
// a relative fixture file.
// Actual Fetch API WPTs are executed in nodejs/undici.
globalThis.fetch = function fetch(file) {
  return resource.readAsFetch(workerData.testRelativePath, file);
};

if (workerData.initScript) {
  runInThisContext(workerData.initScript, {
    importModuleDynamically: USE_MAIN_CONTEXT_DEFAULT_LOADER,
  });
}

runInThisContext(workerData.harness.code, {
  filename: workerData.harness.filename,
  importModuleDynamically: USE_MAIN_CONTEXT_DEFAULT_LOADER,
});

// If there are skip patterns, wrap test functions to prevent execution of
// matching tests. This must happen after testharness.js is loaded but before
// the test scripts run.
if (workerData.skippedTests?.length) {
  function isSkipped(name) {
    for (const matcher of workerData.skippedTests) {
      if (typeof matcher === 'string') {
        if (name === matcher) return true;
      } else if (matcher.test(name)) {
        return true;
      }
    }
    return false;
  }
  for (const fn of ['test', 'async_test', 'promise_test']) {
    const original = globalThis[fn];
    globalThis[fn] = function(func, name, ...rest) {
      if (typeof name === 'string' && isSkipped(name)) {
        parentPort.postMessage({ type: 'skip', name });
        return;
      }
      return original.call(this, func, name, ...rest);
    };
  }
}

// eslint-disable-next-line no-undef
add_result_callback((result) => {
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

// Keep the event loop alive
const timeout = setTimeout(() => {
  parentPort.postMessage({
    type: 'completion',
    status: { status: 2 },
  });
}, 2 ** 31 - 1); // Max timeout is 2^31-1, when overflown the timeout is set to 1.

// eslint-disable-next-line no-undef
add_completion_callback((_, status) => {
  clearTimeout(timeout);
  parentPort.postMessage({
    type: 'completion',
    status,
  });
});

for (const scriptToRun of workerData.scriptsToRun) {
  runInThisContext(scriptToRun.code, {
    filename: scriptToRun.filename,
    importModuleDynamically: USE_MAIN_CONTEXT_DEFAULT_LOADER,
  });
}
