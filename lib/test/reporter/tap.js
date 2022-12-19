'use strict';
const {
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ObjectEntries,
  RegExpPrototypeSymbolReplace,
  SafeMap,
  StringPrototypeReplaceAll,
  StringPrototypeSplit,
  StringPrototypeRepeat,
} = primordials;
const { inspectWithNoCustomRetry } = require('internal/errors');
const { isError, kEmptyObject } = require('internal/util');
const kDefaultIndent = '    '; // 4 spaces
const kFrameStartRegExp = /^ {4}at /;
const kLineBreakRegExp = /\n|\r\n/;
const kDefaultTAPVersion = 13;
const inspectOptions = { colors: false, breakLength: Infinity };
let testModule; // Lazy loaded due to circular dependency.

function lazyLoadTest() {
  testModule ??= require('internal/test_runner/test');
  return testModule;
}


async function * tapReporter(source) {
  yield `TAP version ${kDefaultTAPVersion}\n`;
  for await (const { type, data } of source) {
    switch (type) {
      case 'test:fail':
        yield reportTest(data.nesting, data.testNumber, 'not ok', data.name, data.skip, data.todo);
        yield reportDetails(data.nesting, data.details);
        break;
      case 'test:pass':
        yield reportTest(data.nesting, data.testNumber, 'ok', data.name, data.skip, data.todo);
        yield reportDetails(data.nesting, data.details);
        break;
      case 'test:plan':
        yield `${indent(data.nesting)}1..${data.count}\n`;
        break;
      case 'test:start':
        yield `${indent(data.nesting)}# Subtest: ${tapEscape(data.name)}\n`;
        break;
      case 'test:diagnostic':
        yield `${indent(data.nesting)}# ${tapEscape(data.message)}\n`;
        break;
    }
  }
}

function reportTest(nesting, testNumber, status, name, skip, todo) {
  let line = `${indent(nesting)}${status} ${testNumber}`;

  if (name) {
    line += ` ${tapEscape(`- ${name}`)}`;
  }

  if (skip !== undefined) {
    line += ` # SKIP${typeof skip === 'string' && skip.length ? ` ${tapEscape(skip)}` : ''}`;
  } else if (todo !== undefined) {
    line += ` # TODO${typeof todo === 'string' && todo.length ? ` ${tapEscape(todo)}` : ''}`;
  }

  line += '\n';

  return line;
}


function reportDetails(nesting, data = kEmptyObject) {
  const { error, duration_ms } = data;
  const _indent = indent(nesting);
  let details = `${_indent}  ---\n`;

  details += jsToYaml(_indent, 'duration_ms', duration_ms);
  details += jsToYaml(_indent, null, error);
  details += `${_indent}  ...\n`;
  return details;
}

const memo = new SafeMap();
function indent(nesting) {
  let value = memo.get(nesting);
  if (value === undefined) {
    value = StringPrototypeRepeat(kDefaultIndent, nesting);
    memo.set(nesting, value);
  }

  return value;
}


// In certain places, # and \ need to be escaped as \# and \\.
function tapEscape(input) {
  let result = StringPrototypeReplaceAll(input, '\\', '\\\\');
  result = StringPrototypeReplaceAll(result, '#', '\\#');
  result = StringPrototypeReplaceAll(result, '\b', '\\b');
  result = StringPrototypeReplaceAll(result, '\f', '\\f');
  result = StringPrototypeReplaceAll(result, '\t', '\\t');
  result = StringPrototypeReplaceAll(result, '\n', '\\n');
  result = StringPrototypeReplaceAll(result, '\r', '\\r');
  result = StringPrototypeReplaceAll(result, '\v', '\\v');
  return result;
}

function jsToYaml(indent, name, value) {
  if (value === null || value === undefined) {
    return '';
  }

  if (typeof value !== 'object') {
    const prefix = `${indent}  ${name}: `;

    if (typeof value !== 'string') {
      return `${prefix}${inspectWithNoCustomRetry(value, inspectOptions)}\n`;
    }

    const lines = StringPrototypeSplit(value, kLineBreakRegExp);

    if (lines.length === 1) {
      return `${prefix}${inspectWithNoCustomRetry(value, inspectOptions)}\n`;
    }

    let str = `${prefix}|-\n`;

    for (let i = 0; i < lines.length; i++) {
      str += `${indent}    ${lines[i]}\n`;
    }

    return str;
  }

  const entries = ObjectEntries(value);
  const isErrorObj = isError(value);
  let result = '';

  for (let i = 0; i < entries.length; i++) {
    const { 0: key, 1: value } = entries[i];

    if (isErrorObj && (key === 'cause' || key === 'code')) {
      continue;
    }

    result += jsToYaml(indent, key, value);
  }

  if (isErrorObj) {
    const { kTestCodeFailure, kUnwrapErrors } = lazyLoadTest();
    const {
      cause,
      code,
      failureType,
      message,
      expected,
      actual,
      operator,
      stack,
    } = value;
    let errMsg = message ?? '<unknown error>';
    let errStack = stack;
    let errCode = code;
    let errExpected = expected;
    let errActual = actual;
    let errOperator = operator;
    let errIsAssertion = isAssertionLike(value);

    // If the ERR_TEST_FAILURE came from an error provided by user code,
    // then try to unwrap the original error message and stack.
    if (code === 'ERR_TEST_FAILURE' && kUnwrapErrors.has(failureType)) {
      errStack = cause?.stack ?? errStack;
      errCode = cause?.code ?? errCode;
      if (isAssertionLike(cause)) {
        errExpected = cause.expected;
        errActual = cause.actual;
        errOperator = cause.operator ?? errOperator;
        errIsAssertion = true;
      }
      if (failureType === kTestCodeFailure) {
        errMsg = cause?.message ?? errMsg;
      }
    }

    result += jsToYaml(indent, 'error', errMsg);

    if (errCode) {
      result += jsToYaml(indent, 'code', errCode);
    }

    if (errIsAssertion) {
      result += jsToYaml(indent, 'expected', errExpected);
      result += jsToYaml(indent, 'actual', errActual);
      if (errOperator) {
        result += jsToYaml(indent, 'operator', errOperator);
      }
    }

    if (typeof errStack === 'string') {
      const frames = [];

      ArrayPrototypeForEach(
        StringPrototypeSplit(errStack, kLineBreakRegExp),
        (frame) => {
          const processed = RegExpPrototypeSymbolReplace(
            kFrameStartRegExp,
            frame,
            ''
          );

          if (processed.length > 0 && processed.length !== frame.length) {
            ArrayPrototypePush(frames, processed);
          }
        }
      );

      if (frames.length > 0) {
        const frameDelimiter = `\n${indent}    `;

        result += `${indent}  stack: |-${frameDelimiter}`;
        result += `${ArrayPrototypeJoin(frames, frameDelimiter)}\n`;
      }
    }
  }

  return result;
}

function isAssertionLike(value) {
  return value && typeof value === 'object' && 'expected' in value && 'actual' in value;
}

module.exports = tapReporter;
