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
const Transform = require('internal/streams/transform');
const { inspectWithNoCustomRetry } = require('internal/errors');
const colors = require('internal/util/colors');
const { kSubtestsFailed } = require('internal/test_runner/test');
const { getCoverageReport } = require('internal/test_runner/utils');
const { relative } = require('path');

const symbols = {
  '__proto__': null,
  'test:fail': '\u2716 ',
  'test:pass': '\u2714 ',
  'test:diagnostic': '\u2139 ',
  'test:coverage': '\u2139 ',
  'arrow:right': '\u25B6 ',
  'hyphen:minus': '\uFE63 ',
};
class SpecReporter extends Transform {
  #stack = [];
  #reported = [];
  #indentMemo = new SafeMap();
  #failedTests = [];
  #cwd = process.cwd();
  #inspectOptions;
  #colors;

  constructor() {
    super({ __proto__: null, writableObjectMode: true });
    colors.refresh();
    this.#inspectOptions = { __proto__: null, colors: colors.shouldColorize(process.stdout), breakLength: Infinity };
    this.#colors = {
      '__proto__': null,
      'test:fail': colors.red,
      'test:pass': colors.green,
      'test:diagnostic': colors.blue,
    };
  }

  #indent(nesting) {
    let value = this.#indentMemo.get(nesting);
    if (value === undefined) {
      value = StringPrototypeRepeat('  ', nesting);
      this.#indentMemo.set(nesting, value);
    }

    return value;
  }
  #formatError(error, indent) {
    if (!error) return '';
    const err = error.code === 'ERR_TEST_FAILURE' ? error.cause : error;
    const message = ArrayPrototypeJoin(
      RegExpPrototypeSymbolSplit(
        hardenRegExp(/\r?\n/),
        inspectWithNoCustomRetry(err, this.#inspectOptions),
      ), `\n${indent}  `);
    return `\n${indent}  ${message}\n`;
  }
  #formatTestReport(type, data, prefix = '', indent = '', hasChildren = false) {
    let color = this.#colors[type] ?? colors.white;
    let symbol = symbols[type] ?? ' ';
    const { skip, todo } = data;
    const duration_ms = data.details?.duration_ms ? ` ${colors.gray}(${data.details.duration_ms}ms)${colors.white}` : '';
    let title = `${data.name}${duration_ms}`;

    if (skip !== undefined) {
      title += ` # ${typeof skip === 'string' && skip.length ? skip : 'SKIP'}`;
    } else if (todo !== undefined) {
      title += ` # ${typeof todo === 'string' && todo.length ? todo : 'TODO'}`;
    }
    const error = this.#formatError(data.details?.error, indent);
    if (hasChildren) {
      // If this test has had children - it was already reported, so slightly modify the output
      const err = data.details?.error?.failureType === 'subtestsFailed' ? '' : error;
      return `${prefix}${indent}${color}${symbols['arrow:right']}${colors.white}${title}\n${err}`;
    }
    if (skip !== undefined) {
      color = colors.gray;
      symbol = symbols['hyphen:minus'];
    }
    return `${prefix}${indent}${color}${symbol}${title}${colors.white}${error}`;
  }
  #handleTestReportEvent(type, data) {
    const subtest = ArrayPrototypeShift(this.#stack); // This is the matching `test:start` event
    if (subtest) {
      assert(subtest.type === 'test:start');
      assert(subtest.data.nesting === data.nesting);
      assert(subtest.data.name === data.name);
    }
    let prefix = '';
    while (this.#stack.length) {
      // Report all the parent `test:start` events
      const parent = ArrayPrototypePop(this.#stack);
      assert(parent.type === 'test:start');
      const msg = parent.data;
      ArrayPrototypeUnshift(this.#reported, msg);
      prefix += `${this.#indent(msg.nesting)}${symbols['arrow:right']}${msg.name}\n`;
    }
    let hasChildren = false;
    if (this.#reported[0] && this.#reported[0].nesting === data.nesting && this.#reported[0].name === data.name) {
      ArrayPrototypeShift(this.#reported);
      hasChildren = true;
    }
    const indent = this.#indent(data.nesting);
    return `${this.#formatTestReport(type, data, prefix, indent, hasChildren)}\n`;
  }
  #handleEvent({ type, data }) {
    switch (type) {
      case 'test:fail':
        if (data.details?.error?.failureType !== kSubtestsFailed) {
          ArrayPrototypePush(this.#failedTests, data);
        }
        return this.#handleTestReportEvent(type, data);
      case 'test:pass':
        return this.#handleTestReportEvent(type, data);
      case 'test:start':
        ArrayPrototypeUnshift(this.#stack, { __proto__: null, data, type });
        break;
      case 'test:stderr':
      case 'test:stdout':
        return data.message;
      case 'test:diagnostic':
        return `${this.#colors[type]}${this.#indent(data.nesting)}${symbols[type]}${data.message}${colors.white}\n`;
      case 'test:coverage':
        return getCoverageReport(this.#indent(data.nesting), data.summary, symbols['test:coverage'], colors.blue, true);
    }
  }
  _transform({ type, data }, encoding, callback) {
    callback(null, this.#handleEvent({ __proto__: null, type, data }));
  }
  _flush(callback) {
    if (this.#failedTests.length === 0) {
      callback(null, '');
      return;
    }
    const results = [`\n${this.#colors['test:fail']}${symbols['test:fail']}failing tests:${colors.white}\n`];
    for (let i = 0; i < this.#failedTests.length; i++) {
      const test = this.#failedTests[i];
      const relPath = relative(this.#cwd, test.file);
      const formattedErr = this.#formatTestReport('test:fail', test);
      const location = `test at ${relPath}:${test.line}:${test.column}`;

      ArrayPrototypePush(results, location, formattedErr);
    }
    callback(null, ArrayPrototypeJoin(results, '\n'));
  }
}

module.exports = SpecReporter;
