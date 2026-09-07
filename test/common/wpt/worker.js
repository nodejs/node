'use strict';

const path = require('path');
const { pathToFileURL } = require('url');
const {
  runInNewContext,
  runInThisContext,
  constants: { USE_MAIN_CONTEXT_DEFAULT_LOADER },
} = require('vm');
const { setFlagsFromString } = require('v8');
const { inspect } = require('util');
const {
  isMainThread,
  parentPort,
  workerData: threadData,
} = require('worker_threads');

// The runner starts this either on a worker thread, where the spec to run
// arrives as workerData, or in a child process, where it arrives over IPC.
const send = isMainThread ?
  (message) => process.send(message) :
  (message) => parentPort.postMessage(message);

if (isMainThread) {
  // A worker thread surfaces uncaught errors through its 'error' event; a
  // process has to report them itself so that the runner names the failure
  // exactly the same way.
  process.on('uncaughtException', (err) => {
    process.send({
      type: 'uncaught',
      error: { name: `${err}`, message: err.message, stack: inspect(err) },
    }, () => process.exit(1));
  });
  process.once('message', run);
} else {
  run(threadData);
}

function run(workerData) {
  const { ResourceLoader } = require(workerData.wptRunner);
  const resource = new ResourceLoader(workerData.wptPath);

  // Tests create workers with URLs the WPT server would have served them
  // from; map them into the fixtures directory.
  const RealWorker = globalThis.Worker;
  globalThis.Worker = class Worker extends RealWorker {
    constructor(url, options) {
      super(resource.mapServerURL(workerData.testRelativePath, url), options);
    }
  };

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
    const isSkipped = (name) => {
      for (const matcher of workerData.skippedTests) {
        if (typeof matcher === 'string') {
          if (name === matcher) return true;
        } else if (matcher.test(name)) {
          return true;
        }
      }
      return false;
    };
    for (const fn of ['test', 'async_test', 'promise_test']) {
      const original = globalThis[fn];
      globalThis[fn] = function(func, name, ...rest) {
        if (typeof name === 'string' && isSkipped(name)) {
          send({ type: 'skip', name });
          return;
        }
        return original.call(this, func, name, ...rest);
      };
    }
  }

  // eslint-disable-next-line no-undef
  add_result_callback((result) => {
    send({
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
    send({
      type: 'completion',
      status: { status: 2 },
    });
  }, 2 ** 31 - 1); // Max timeout is 2^31-1, when overflown the timeout is set to 1.

  // eslint-disable-next-line no-undef
  add_completion_callback((_, status) => {
    clearTimeout(timeout);
    send({
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

  if (workerData.webWorker) {
    const worker = new RealWorker(
      pathToFileURL(path.join(__dirname, 'webworker.js')));
    worker.postMessage({
      wptRunner: workerData.wptRunner,
      wptPath: workerData.wptPath,
      testRelativePath: workerData.testRelativePath,
      ...workerData.webWorker,
    });

    let completed = false;
    worker.addEventListener('message', (event) => {
      if (event.data?.type === 'complete') {
        completed = true;
      }
      // Skipped subtests never register with the testharness inside the
      // worker; the runner is notified about them directly.
      if (event.data?.type === 'skip') {
        send({ type: 'skip', name: event.data.name });
      }
    });
    worker.addEventListener('error', (event) => {
      if (completed) {
        return;
      }
      clearTimeout(timeout);
      send({
        type: 'completion',
        status: {
          status: 1,
          message: event.message,
          stack: event.error?.stack,
        },
      });
    });

    // eslint-disable-next-line no-undef
    fetch_tests_from_worker(worker);
  }
}
