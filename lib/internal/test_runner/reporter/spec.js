'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ArrayPrototypeUnshift,
  hardenRegExp,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  StringPrototypeRepeat,
} = primordials;

const assert = require('assert');
const { inspectWithNoCustomRetry } = require('internal/errors');
const { green, blue, red, white, gray, shouldColorize } = require('internal/util/colors');
const { kSubtestsFailed } = require('internal/test_runner/test');
const { getCoverageReport } = require('internal/test_runner/utils');
const { relative } = require('path');

const inspectOptions = { __proto__: null, colors: shouldColorize(process.stdout), breakLength: Infinity };

const colors = {
  '__proto__': null,
  'test:fail': red,
  'test:pass': green,
  'test:diagnostic': blue,
};

const symbols = {
  '__proto__': null,
  'test:fail': '\u2716 ',
  'test:pass': '\u2714 ',
  'test:diagnostic': '\u2139 ',
  'test:coverage': '\u2139 ',
  'arrow:right': '\u25B6 ',
  'hyphen:minus': '\uFE63 ',
};

module.exports = async function* specReporter(source) {
  let stack = [];
  let reported = [];
  let indentMemo = new SafeMap();
  let failedTests = [];
  let cwd = process.cwd();

  function indent(nesting) {
    let value = indentMemo.get(nesting);
    if (value === undefined) {
      value = StringPrototypeRepeat('  ', nesting);
      indentMemo.set(nesting, value);
    }

    return value;
  }

  function formatError(error, indent) {
    if (!error) return '';
    const err = error.code === 'ERR_TEST_FAILURE' ? error.cause : error;
    const message = ArrayPrototypeJoin(
      RegExpPrototypeSymbolSplit(
        hardenRegExp(/\r?\n/),
        inspectWithNoCustomRetry(err, inspectOptions),
      ), `\n${indent}  `);
    return `\n${indent}  ${message}\n`;
  }

  function formatTestReport(type, data, prefix = '', indent = '', hasChildren = false) {
    let color = colors[type] ?? white;
    let symbol = symbols[type] ?? ' ';
    const { skip, todo } = data;
    const duration_ms = data.details?.duration_ms ? ` ${gray}(${data.details.duration_ms}ms)${white}` : '';
    let title = `${data.name}${duration_ms}`;

    if (skip !== undefined) {
      title += ` # ${typeof skip === 'string' && skip.length ? skip : 'SKIP'}`;
    } else if (todo !== undefined) {
      title += ` # ${typeof todo === 'string' && todo.length ? todo : 'TODO'}`;
    }
    if (hasChildren) {
      // If this test has had children - it was already reported, so slightly modify the output
      return `${prefix}${indent}${color}${symbols['arrow:right']}${white}${title}\n`;
    }
    const error = formatError(data.details?.error, indent);
    if (skip !== undefined) {
      color = gray;
      symbol = symbols['hyphen:minus'];
    }
    return `${prefix}${indent}${color}${symbol}${title}${white}${error}`;
  }

  function handleTestReportEvent(type, data) {
    const subtest = ArrayPrototypeShift(stack); // This is the matching `test:start` event
    if (subtest) {
      assert(subtest.type === 'test:start');
      assert(subtest.data.nesting === data.nesting);
      assert(subtest.data.name === data.name);
    }
    let prefix = '';
    while (stack.length) {
      // Report all the parent `test:start` events
      const parent = ArrayPrototypePop(stack);
      assert(parent.type === 'test:start');
      const msg = parent.data;
      ArrayPrototypeUnshift(reported, msg);
      prefix += `${indent(msg.nesting)}${symbols['arrow:right']}${msg.name}\n`;
    }
    let hasChildren = false;
    if (reported[0] && reported[0].nesting === data.nesting && reported[0].name === data.name) {
      ArrayPrototypeShift(reported);
      hasChildren = true;
    }
    return `${formatTestReport(type, data, prefix, indent(data.nesting), hasChildren)}\n`;
  }

  function handleEvent({ type, data }) {
    switch (type) {
      case 'test:fail':
        if (data.details?.error?.failureType !== kSubtestsFailed) {
          ArrayPrototypePush(failedTests, data);
        }
        return handleTestReportEvent(type, data);
      case 'test:pass':
        return handleTestReportEvent(type, data);
      case 'test:start':
        ArrayPrototypeUnshift(stack, { __proto__: null, data, type });
        break;
      case 'test:stderr':
      case 'test:stdout':
        return data.message;
      case 'test:diagnostic':
        return `${colors[type]}${indent(data.nesting)}${symbols[type]}${data.message}${white}\n`;
      case 'test:coverage':
        return getCoverageReport(indent(data.nesting), data.summary, symbols['test:coverage'], blue, true);
    }
  }

  for await (const event of source) {
    yield handleEvent(event);
  }

  if (failedTests.length !== 0) {
    const results = [`\n${colors['test:fail']}${symbols['test:fail']}failing tests:${white}\n`];
    for (let i = 0; i < failedTests.length; i++) {
      const test = failedTests[i];
      const relPath = relative(cwd, test.file);
      const formattedErr = formatTestReport('test:fail', test);
      const location = `test at ${relPath}:${test.line}:${test.column}`;

      ArrayPrototypePush(results, location, formattedErr);
    }
    yield ArrayPrototypeJoin(results, '\n');
  }
};
