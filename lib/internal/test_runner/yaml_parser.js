'use strict';
const {
  codes: {
    ERR_TEST_FAILURE,
  }
} = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const {
  ArrayPrototypeJoin,
  Error,
  Number,
  NumberIsNaN,
  RegExpPrototypeExec,
  StringPrototypeEndsWith,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeSubstring,
} = primordials;

const kYamlKeyRegex = /^(\s+)?(\w+):(\s)+([>|][-+])?(.*)$/;
const kStackDelimiter = '    at ';

function reConstructError(parsedYaml) {
  if (!('error' in parsedYaml)) {
    return parsedYaml;
  }
  const isAssertionError = parsedYaml.code === 'ERR_ASSERTION' ||
    'actual' in parsedYaml || 'expected' in parsedYaml || 'operator' in parsedYaml;
  const isTestFauilure = parsedYaml.code === 'ERR_TEST_FAILURE' || 'failureType' in parsedYaml;
  const stack = parsedYaml.stack ? kStackDelimiter + ArrayPrototypeJoin(parsedYaml.stack, `\n${kStackDelimiter}`) : '';

  // eslint-disable-next-line no-restricted-syntax
  let error = new Error(parsedYaml.error);
  error.stack = stack;
  error.code = parsedYaml.code;

  if (isAssertionError) {
    error = new AssertionError({
      message: parsedYaml.error,
      actual: parsedYaml.actual,
      expected: parsedYaml.expected,
      operator: parsedYaml.operator
    });
    error.stack = stack;
  }

  if (isTestFauilure) {
    error = new ERR_TEST_FAILURE(error, parsedYaml.failureType);
    error.stack = stack;
  }

  parsedYaml.error = error;
  delete parsedYaml.stack;
  delete parsedYaml.code;
  delete parsedYaml.failureType;
  delete parsedYaml.actual;
  delete parsedYaml.expected;
  delete parsedYaml.operator;

  return parsedYaml;
}

function getYamlValue(value, key) {
  if (StringPrototypeStartsWith(value, "'") && StringPrototypeEndsWith(value, "'")) {
    return StringPrototypeSlice(value, 1, -1);
  }
  if (value === 'true') {
    return true;
  }
  if (value === 'false') {
    return false;
  }
  if (value && !NumberIsNaN(value)) {
    return Number(value);
  }
  return value;
}

function parseYAML(lines) {
  const result = {};
  let isInYamlBlock = false;
  for (const line of lines) {
    if (isInYamlBlock && !StringPrototypeStartsWith(line, StringPrototypeRepeat(' ', isInYamlBlock.indent))) {
      result[isInYamlBlock.key] = isInYamlBlock.key === 'stack' ?
        result[isInYamlBlock.key] : ArrayPrototypeJoin(result[isInYamlBlock.key], '\n');
      isInYamlBlock = false;
    }
    if (isInYamlBlock) {
      const blockLine = StringPrototypeSubstring(line, isInYamlBlock.indent);
      result[isInYamlBlock.key].push(blockLine);
      continue;
    }
    const match = RegExpPrototypeExec(kYamlKeyRegex, line);
    if (match !== null) {
      const { 1: leadingSpaces, 2: key, 4: block, 5: value } = match;
      if (block) {
        isInYamlBlock = { key, indent: (leadingSpaces?.length ?? 0) + 2 };
        result[key] = [];
      } else {
        result[key] = getYamlValue(value, key);
      }
    }
  }
  return reConstructError(result);
}

module.exports = {
  parseYAML,
};
