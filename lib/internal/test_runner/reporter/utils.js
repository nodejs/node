'use strict';
const {
  ArrayPrototypeJoin,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  StringPrototypeRepeat,
  hardenRegExp,
} = primordials;
const colors = require('internal/util/colors');
const colorsTheme = require('internal/test_runner/colors');
const { inspectWithNoCustomRetry } = require('internal/errors');
const indentMemo = new SafeMap();

const inspectOptions = {
  __proto__: null,
  colors: colors.shouldColorize(process.stdout),
  breakLength: Infinity,
};

const reporterUnicodeSymbolMap = {
  '__proto__': null,
  'test:fail': '\u2716 ',
  'test:pass': '\u2714 ',
  'test:diagnostic': '\u2139 ',
  'test:coverage': '\u2139 ',
  'arrow:right': '\u25B6 ',
  'hyphen:minus': '\uFE63 ',
};

const reporterColorMap = {
  '__proto__': null,
  get 'test:fail'() {
    return colorsTheme.fail;
  },
  get 'test:pass'() {
    return colorsTheme.pass;
  },
  get 'test:diagnostic'() {
    return colorsTheme.info;
  },
  get 'info'() {
    return colorsTheme.info;
  },
  get 'warn'() {
    return colorsTheme.warn;
  },
  get 'error'() {
    return colorsTheme.fail;
  },
};

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

function formatTestReport(type, data, prefix = '', indent = '', hasChildren = false, showErrorDetails = true) {
  let color = reporterColorMap[type] ?? colorsTheme.base;
  let symbol = reporterUnicodeSymbolMap[type] ?? ' ';
  const { skip, todo } = data;
  const duration_ms = data.details?.duration_ms ? ` ${colorsTheme.duration}(${data.details.duration_ms}ms)${colorsTheme.base}` : '';
  let title = `${data.name}${duration_ms}`;

  if (skip !== undefined) {
    title += ` # ${typeof skip === 'string' && skip.length ? skip : 'SKIP'}`;
  } else if (todo !== undefined) {
    title += ` # ${typeof todo === 'string' && todo.length ? todo : 'TODO'}`;
  }

  const error = showErrorDetails ? formatError(data.details?.error, indent) : '';
  const err = hasChildren ?
    (!error || data.details?.error?.failureType === 'subtestsFailed' ? '' : `\n${error}`) :
    error;

  if (skip !== undefined) {
    color = colorsTheme.duration;
    symbol = reporterUnicodeSymbolMap['hyphen:minus'];
  }
  return `${prefix}${indent}${color}${symbol}${title}${colorsTheme.base}${err}`;
}

module.exports = {
  __proto__: null,
  reporterUnicodeSymbolMap,
  reporterColorMap,
  formatTestReport,
  indent,
};
