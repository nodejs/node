'use strict';

// Runs a WPT test file inside a Web Worker
// Refs: https://web-platform-tests.org/writing-tests/testharness.html

const { pathToFileURL } = require('url');
const {
  runInThisContext,
  constants: { USE_MAIN_CONTEXT_DEFAULT_LOADER },
} = require('vm');

globalThis.onmessage = ({ data }) => {
  // Let the test install its own handler.
  globalThis.onmessage = null;

  const { ResourceLoader } = require(data.wptRunner);
  const resource = new ResourceLoader(data.wptPath);

  globalThis.fetch = function fetch(file) {
    return resource.readAsFetch(data.testRelativePath, file);
  };

  // Pretend the worker was served from the URL the WPT server would have
  // used
  const fakePath = (data.isAnyTest ?
    data.testRelativePath.replace(/\.any\.js$/, '.any.worker.js') :
    data.testRelativePath).replace(/\\/g, '/');
  const fakeURL = new URL(`/${fakePath}${data.variant}`, 'http://wpt');
  // eslint-disable-next-line no-undef
  const fakeLocation = { __proto__: WorkerLocation.prototype };
  for (const key of ['href', 'origin', 'protocol', 'host', 'hostname',
                     'port', 'pathname', 'search', 'hash']) {
    Object.defineProperty(fakeLocation, key, {
      value: fakeURL[key],
      enumerable: true,
    });
  }
  Object.defineProperty(fakeLocation, 'toString', {
    value: function toString() { return fakeURL.href; },
    enumerable: true,
  });
  Object.defineProperty(globalThis, 'location', {
    value: fakeLocation,
    enumerable: true,
    configurable: true,
  });

  const testharnessPath =
    pathToFileURL(resource.toRealFilePath(data.testRelativePath,
                                          '/resources/testharness.js')).href;

  // If there are skip patterns, wrap the test functions to prevent
  // execution of matching tests. This must happen after testharness.js is
  // loaded but before the test scripts run.
  function applySkips() {
    if (!data.skippedTests?.length) {
      return;
    }
    function isSkipped(name) {
      for (const matcher of data.skippedTests) {
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
          // eslint-disable-next-line no-undef
          postMessage({ type: 'skip', name });
          return;
        }
        return original.call(this, func, name, ...rest);
      };
    }
  }

  // Tests fetch scripts and nested worker scripts from the WPT server; map
  // those URLs into the fixtures directory.
  const realImportScripts = globalThis.importScripts;
  globalThis.importScripts = function importScripts(...urls) {
    const mapped = urls.map(
      (url) => resource.mapServerURL(data.testRelativePath, url));
    const result = realImportScripts.apply(this, mapped);
    if (mapped.includes(testharnessPath)) {
      applySkips();
    }
    return result;
  };
  const RealWorker = globalThis.Worker;
  globalThis.Worker = class Worker extends RealWorker {
    constructor(url, options) {
      super(resource.mapServerURL(data.testRelativePath, url), options);
    }
  };

  if (data.isAnyTest) {
    // Emulate the generated .any.worker.js wrapper script.
    // Refs: https://github.com/web-platform-tests/wpt/blob/master/tools/serve/serve.py
    globalThis.GLOBAL = {
      isWindow() { return false; },
      isWorker() { return true; },
      isShadowRealm() { return false; },
    };
  }

  if (data.initScript) {
    runInThisContext(data.initScript, {
      importModuleDynamically: USE_MAIN_CONTEXT_DEFAULT_LOADER,
    });
  }

  if (data.title) {
    globalThis.META_TITLE = data.title;
  }

  if (data.isAnyTest) {
    globalThis.importScripts('/resources/testharness.js');
    for (const script of data.scripts) {
      globalThis.importScripts(pathToFileURL(script).href);
    }
    globalThis.importScripts(pathToFileURL(data.path).href);
    // eslint-disable-next-line no-undef
    done();
  } else {
    // *.worker.js tests import testharness.js and call done() themselves.
    globalThis.importScripts(pathToFileURL(data.path).href);
  }
};
