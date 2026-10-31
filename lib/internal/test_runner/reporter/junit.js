'use strict';
const {
  ArrayPrototypeFilter,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeSome,
  Date,
  DateNow,
  DatePrototypeToISOString,
  NumberPrototypeToFixed,
  ObjectEntries,
  RegExpPrototypeSymbolReplace,
  String,
  StringPrototypeRepeat,
} = primordials;

const { inspectWithNoCustomRetry } = require('internal/errors');
const { stripVTControlCharacters } = require('internal/util/inspect');
const { hostname } = require('os');

const inspectOptions = { __proto__: null, colors: false, breakLength: Infinity };
const HOSTNAME = hostname();

// Characters that are not allowed anywhere in an XML 1.0 document, not even
// as character references: C0 controls other than tab, LF and CR, lone
// surrogates, and U+FFFE/U+FFFF.
const kInvalidXMLChars = /[^\t\n\r\x20-\uD7FF\uE000-\uFFFD\u{10000}-\u{10FFFF}]/gu;

function sanitize(s = '') {
  // Drop ANSI escape sequences entirely (e.g. colored assertion diffs), then
  // replace any remaining illegal characters with U+FFFD.
  return RegExpPrototypeSymbolReplace(kInvalidXMLChars, stripVTControlCharacters(s), '\uFFFD');
}

function escapeAttribute(s = '') {
  return RegExpPrototypeSymbolReplace(/"/g, RegExpPrototypeSymbolReplace(/\n/g, escapeContent(s), '&#10;'), '&quot;');
}

function escapeContent(s = '') {
  return RegExpPrototypeSymbolReplace(/</g, RegExpPrototypeSymbolReplace(/&/g, sanitize(s), '&amp;'), '&lt;');
}

function escapeComment(s = '') {
  return RegExpPrototypeSymbolReplace(/--/g, sanitize(s), '&#45;&#45;');
}

function treeToXML(tree) {
  if (typeof tree === 'string') {
    return `${escapeContent(tree)}\n`;
  }
  const {
    tag, attrs, nesting, children, comment,
  } = tree;
  const indent = StringPrototypeRepeat('\t', nesting + 1);
  if (comment != null) {
    return `${indent}<!-- ${escapeComment(comment)} -->\n`;
  }
  const attrsString = ArrayPrototypeJoin(
    ArrayPrototypeMap(
      ObjectEntries(attrs),
      ({ 0: key, 1: value }) => `${key}="${escapeAttribute(String(value))}"`),
    ' ');
  if (!children?.length) {
    return `${indent}<${tag} ${attrsString}/>\n`;
  }
  const childrenString = ArrayPrototypeJoin(ArrayPrototypeMap(children ?? [], treeToXML), '');
  return `${indent}<${tag} ${attrsString}>\n${childrenString}${indent}</${tag}>\n`;
}

function isFailure(node) {
  return (node?.children && ArrayPrototypeSome(node.children, (c) => c.tag === 'failure')) || node?.attrs?.failures;
}

function isSkipped(node) {
  return (node?.children && ArrayPrototypeSome(node.children, (c) => c.tag === 'skipped')) || node?.attrs?.skipped;
}

function setSuiteAttrs(node, tests) {
  node.tag = 'testsuite';
  node.attrs.disabled = 0;
  node.attrs.errors = 0;
  node.attrs.tests = tests;
  node.attrs.failures = ArrayPrototypeFilter(node.children, isFailure).length;
  node.attrs.skipped = ArrayPrototypeFilter(node.children, isSkipped).length;
}

// Give a test that started but never reported a result (e.g. because a test
// child process exited before all of its output was read) a valid element.
// A leaf is reported as a failure so that lost results don't look green.
function closeIncomplete(node) {
  const tests = ArrayPrototypeFilter(node.children, (c) => c.comment == null).length;
  if (tests > 0) {
    setSuiteAttrs(node, tests);
    node.attrs.hostname = HOSTNAME;
    return;
  }
  const message = 'test did not report a result';
  node.tag = 'testcase';
  node.attrs.classname = 'test';
  node.attrs.failure = message;
  ArrayPrototypePush(node.children, {
    __proto__: null, nesting: node.nesting + 1, tag: 'failure',
    attrs: { __proto__: null, type: 'incomplete', message },
  });
}

module.exports = async function* junitReporter(source) {
  yield '<?xml version="1.0" encoding="utf-8"?>\n';
  yield '<testsuites>\n';
  let currentSuite = null;
  const roots = [];

  // Close any unfinished tests nested at `nesting` or deeper, so that later
  // results are not attached to a test whose own result never arrived.
  function closeUnfinished(nesting) {
    while (currentSuite != null && currentSuite.nesting >= nesting) {
      closeIncomplete(currentSuite);
      currentSuite = currentSuite.parent;
    }
  }

  function startTest(event) {
    closeUnfinished(event.data.nesting);
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
        closeUnfinished(event.data.nesting + 1);
        if (currentSuite == null ||
          currentSuite.attrs.name !== event.data.name ||
          currentSuite.nesting !== event.data.nesting) {
          // No matching test:start was seen; start the test now.
          startTest(event);
        }
        const currentTest = currentSuite;
        if (currentSuite?.nesting === event.data.nesting) {
          currentSuite = currentSuite.parent;
        }
        currentTest.attrs.time = NumberPrototypeToFixed(event.data.details.duration_ms / 1000, 6);
        const nonCommentChildren = ArrayPrototypeFilter(currentTest.children, (c) => c.comment == null);
        if (nonCommentChildren.length > 0) {
          setSuiteAttrs(currentTest, nonCommentChildren.length);
          // A suite's `test:start` is emitted lazily (when its first subtest
          // reports), so derive the start time from the end minus the measured
          // duration rather than stamping the (late) test:start moment.
          currentTest.attrs.timestamp =
            DatePrototypeToISOString(new Date(DateNow() - event.data.details.duration_ms));
          currentTest.attrs.hostname = HOSTNAME;
        } else {
          currentTest.tag = 'testcase';
          currentTest.attrs.classname = event.data.classname ?? 'test';
          if (event.data.file) {
            currentTest.attrs.file = event.data.file;
          }
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
              attrs: { __proto__: null, type: error?.failureType || error?.code, message: error?.message.trim() ?? '' },
              children: [inspectWithNoCustomRetry(error, inspectOptions)],
            });
            currentTest.failures = 1;
            currentTest.attrs.failure = error?.message ?? '';
          }
        }
        break;
      }
      case 'test:diagnostic':
      case 'test:log': {
        const parent = currentSuite?.children ?? roots;
        ArrayPrototypePush(parent, {
          __proto__: null, nesting: event.data.nesting, comment: event.data.message,
        });
        break;
      } default:
        break;
    }
  }
  closeUnfinished(0);
  for (const suite of roots) {
    yield treeToXML(suite);
  }
  yield '</testsuites>\n';
};
