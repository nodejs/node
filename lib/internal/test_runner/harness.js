'use strict';
const {
  ArrayPrototypeForEach,
  FunctionPrototypeBind,
  SafeMap,
} = primordials;
const {
  createHook,
  executionAsyncId,
} = require('async_hooks');
const {
  codes: {
    ERR_TEST_FAILURE,
  },
} = require('internal/errors');
const { getOptionValue } = require('internal/options');
const { Test, ItTest, Suite } = require('internal/test_runner/test');

const isTestRunner = getOptionValue('--test');
const testResources = new SafeMap();
const root = new Test({ __proto__: null, name: '<root>' });
let wasRootSetup = false;

function createProcessEventHandler(eventName, rootTest) {
  return (err) => {
    // Check if this error is coming from a test. If it is, fail the test.
    const test = testResources.get(executionAsyncId());

    if (!test) {
      throw err;
    }

    if (test.finished) {
      // If the test is already finished, report this as a top level
      // diagnostic since this is a malformed test.
      const msg = `Warning: Test "${test.name}" generated asynchronous ` +
        'activity after the test ended. This activity created the error ' +
        `"${err}" and would have caused the test to fail, but instead ` +
        `triggered an ${eventName} event.`;

      rootTest.diagnostic(msg);
      return;
    }

    test.fail(new ERR_TEST_FAILURE(err, eventName));
    test.postRun();
  };
}

function setup(root) {
  if (wasRootSetup) {
    return root;
  }
  const hook = createHook({
    init(asyncId, type, triggerAsyncId, resource) {
      if (resource instanceof Test) {
        testResources.set(asyncId, resource);
        return;
      }

      const parent = testResources.get(triggerAsyncId);

      if (parent !== undefined) {
        testResources.set(asyncId, parent);
      }
    },
    destroy(asyncId) {
      testResources.delete(asyncId);
    }
  });

  hook.enable();

  const exceptionHandler =
    createProcessEventHandler('uncaughtException', root);
  const rejectionHandler =
    createProcessEventHandler('unhandledRejection', root);

  const exitHandler = () => {
    root.postRun();

    let passCount = 0;
    let failCount = 0;
    let skipCount = 0;
    let todoCount = 0;
    let cancelledCount = 0;

    for (let i = 0; i < root.subtests.length; i++) {
      const test = root.subtests[i];

      // Check SKIP and TODO tests first, as those should not be counted as
      // failures.
      if (test.skipped) {
        skipCount++;
      } else if (test.isTodo) {
        todoCount++;
      } else if (test.cancelled) {
        cancelledCount++;
      } else if (!test.passed) {
        failCount++;
      } else {
        passCount++;
      }
    }

    root.reporter.plan(root.indent, root.subtests.length);

    for (let i = 0; i < root.diagnostics.length; i++) {
      root.reporter.diagnostic(root.indent, root.diagnostics[i]);
    }

    root.reporter.diagnostic(root.indent, `tests ${root.subtests.length}`);
    root.reporter.diagnostic(root.indent, `pass ${passCount}`);
    root.reporter.diagnostic(root.indent, `fail ${failCount}`);
    root.reporter.diagnostic(root.indent, `cancelled ${cancelledCount}`);
    root.reporter.diagnostic(root.indent, `skipped ${skipCount}`);
    root.reporter.diagnostic(root.indent, `todo ${todoCount}`);
    root.reporter.diagnostic(root.indent, `duration_ms ${process.uptime()}`);

    root.reporter.push(null);
    hook.disable();
    process.removeListener('unhandledRejection', rejectionHandler);
    process.removeListener('uncaughtException', exceptionHandler);

    if (failCount > 0 || cancelledCount > 0) {
      process.exitCode = 1;
    }
  };

  const terminationHandler = () => {
    exitHandler();
    process.exit();
  };

  process.on('uncaughtException', exceptionHandler);
  process.on('unhandledRejection', rejectionHandler);
  process.on('beforeExit', exitHandler);
  // TODO(MoLow): Make it configurable to hook when isTestRunner === false.
  if (isTestRunner) {
    process.on('SIGINT', terminationHandler);
    process.on('SIGTERM', terminationHandler);
  }

  root.reporter.pipe(process.stdout);
  root.reporter.version();

  wasRootSetup = true;
  return root;
}

function test(name, options, fn) {
  const subtest = setup(root).createSubtest(Test, name, options, fn);
  return subtest.start();
}

function runInParentContext(Factory) {
  function run(name, options, fn, overrides) {
    const parent = testResources.get(executionAsyncId()) || setup(root);
    const subtest = parent.createSubtest(Factory, name, options, fn, overrides);
    if (parent === root) {
      subtest.start();
    }
  }

  const cb = (name, options, fn) => {
    run(name, options, fn);
  };

  ArrayPrototypeForEach(['skip', 'todo'], (keyword) => {
    cb[keyword] = (name, options, fn) => {
      run(name, options, fn, { [keyword]: true });
    };
  });
  return cb;
}

function hook(hook) {
  return (fn, options) => {
    const parent = testResources.get(executionAsyncId()) || setup(root);
    parent.createHook(hook, fn, options);
  };
}

module.exports = {
  test: FunctionPrototypeBind(test, root),
  describe: runInParentContext(Suite),
  it: runInParentContext(ItTest),
  before: hook('before'),
  after: hook('after'),
  beforeEach: hook('beforeEach'),
  afterEach: hook('afterEach'),
};
