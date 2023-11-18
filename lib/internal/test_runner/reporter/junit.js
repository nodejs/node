'use strict';
const {
  ArrayPrototypeFilter,
  ArrayPrototypePush,
  ArrayPrototypeSome,
  NumberPrototypeToFixed,
} = primordials;

const { inspectWithNoCustomRetry } = require('internal/errors');
const { hostname } = require('os');
const { treeToXML } = require('internal/test_runner/utils');

const inspectOptions = { __proto__: null, colors: false, breakLength: Infinity };
const HOSTNAME = hostname();

function isFailure(node) {
  return (node?.children && ArrayPrototypeSome(node.children, (c) => c.tag === 'failure')) || node?.attrs?.failures;
}

function isSkipped(node) {
  return (node?.children && ArrayPrototypeSome(node.children, (c) => c.tag === 'skipped')) || node?.attrs?.failures;
}

module.exports = async function* junitReporter(source) {
  yield '<?xml version="1.0" encoding="utf-8"?>\n';
  yield '<testsuites>\n';
  let currentSuite = null;
  const roots = [];

  function startTest(event) {
    const originalSuite = currentSuite;
    currentSuite = {
      __proto__: null,
      attrs: { __proto__: null, name: event.data.name },
      nesting: event.data.nesting,
      parent: currentSuite,
      children: [],
    };
    if (originalSuite?.children) {
      ArrayPrototypePush(originalSuite.children, currentSuite);
    }
    if (!currentSuite.parent) {
      ArrayPrototypePush(roots, currentSuite);
    }
  }

  for await (const event of source) {
    switch (event.type) {
      case 'test:start': {
        startTest(event);
        break;
      }
      case 'test:pass':
      case 'test:fail': {
        if (!currentSuite) {
          startTest({ __proto__: null, data: { __proto__: null, name: 'root', nesting: 0 } });
        }
        if (currentSuite.attrs.name !== event.data.name ||
          currentSuite.nesting !== event.data.nesting) {
          startTest(event);
        }
        const currentTest = currentSuite;
        if (currentSuite?.nesting === event.data.nesting) {
          currentSuite = currentSuite.parent;
        }
        currentTest.attrs.time = NumberPrototypeToFixed(event.data.details.duration_ms / 1000, 6);
        const nonCommentChildren = ArrayPrototypeFilter(currentTest.children, (c) => c.comment == null);
        if (nonCommentChildren.length > 0) {
          currentTest.tag = 'testsuite';
          currentTest.attrs.disabled = 0;
          currentTest.attrs.errors = 0;
          currentTest.attrs.tests = nonCommentChildren.length;
          currentTest.attrs.failures = ArrayPrototypeFilter(currentTest.children, isFailure).length;
          currentTest.attrs.skipped = ArrayPrototypeFilter(currentTest.children, isSkipped).length;
          currentTest.attrs.hostname = HOSTNAME;
        } else {
          currentTest.tag = 'testcase';
          currentTest.attrs.classname = event.data.classname ?? 'test';
          if (event.data.skip) {
            ArrayPrototypePush(currentTest.children, {
              __proto__: null, nesting: event.data.nesting + 1, tag: 'skipped',
              attrs: { __proto__: null, type: 'skipped', message: event.data.skip },
            });
          }
          if (event.data.todo) {
            ArrayPrototypePush(currentTest.children, {
              __proto__: null, nesting: event.data.nesting + 1, tag: 'skipped',
              attrs: { __proto__: null, type: 'todo', message: event.data.todo },
            });
          }
          if (event.type === 'test:fail') {
            const error = event.data.details?.error;
            currentTest.children.push({
              __proto__: null,
              nesting: event.data.nesting + 1,
              tag: 'failure',
              attrs: { __proto__: null, type: error?.failureType || error?.code, message: error?.message ?? '' },
              children: [inspectWithNoCustomRetry(error, inspectOptions)],
            });
            currentTest.failures = 1;
            currentTest.attrs.failure = error?.message ?? '';
          }
        }
        break;
      }
      case 'test:diagnostic': {
        const parent = currentSuite?.children ?? roots;
        ArrayPrototypePush(parent, {
          __proto__: null, nesting: event.data.nesting, comment: event.data.message,
        });
        break;
      } default:
        break;
    }
  }
  for (const suite of roots) {
    yield treeToXML(suite);
  }
  yield '</testsuites>\n';
};
