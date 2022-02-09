'use strict';
const { FunctionPrototypeBind, SafeMap } = primordials;
const {
  createHook,
  executionAsyncId,
} = require('async_hooks');
const {
  codes: {
    ERR_TEST_FAILURE,
  },
} = require('internal/errors');
const { Test } = require('internal/test_runner/test');

function createProcessEventHandler(eventName, rootTest, testResources) {
  return (err) => {
    // Check if this error is coming from a test. If it is, fail the test.
    const test = testResources.get(executionAsyncId());

    if (test !== undefined) {
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
    }
  };
}

function setup(root) {
  const testResources = new SafeMap();
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
    createProcessEventHandler('uncaughtException', root, testResources);
  const rejectionHandler =
    createProcessEventHandler('unhandledRejection', root, testResources);

  process.on('uncaughtException', exceptionHandler);
  process.on('unhandledRejection', rejectionHandler);
  process.on('beforeExit', () => {
    root.postRun();

    let passCount = 0;
    let failCount = 0;
    let skipCount = 0;
    let todoCount = 0;

    for (let i = 0; i < root.subtests.length; i++) {
      const test = root.subtests[i];

      // Check SKIP and TODO tests first, as those should not be counted as
      // failures.
      if (test.skipped) {
        skipCount++;
      } else if (test.isTodo) {
        todoCount++;
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
    root.reporter.diagnostic(root.indent, `skipped ${skipCount}`);
    root.reporter.diagnostic(root.indent, `todo ${todoCount}`);
    root.reporter.diagnostic(root.indent, `duration_ms ${process.uptime()}`);

    root.reporter.push(null);
    hook.disable();
    process.removeListener('unhandledRejection', rejectionHandler);
    process.removeListener('uncaughtException', exceptionHandler);

    if (failCount > 0) {
      process.exitCode = 1;
    }
  });

  root.reporter.pipe(process.stdout);
  root.reporter.version();
}

function test(name, options, fn) {
  // If this is the first test encountered, bootstrap the test harness.
  if (this.subtests.length === 0) {
    setup(this);
  }

  const subtest = this.createSubtest(name, options, fn);

  return subtest.start();
}

const root = new Test({ name: '<root>' });

module.exports = FunctionPrototypeBind(test, root);
