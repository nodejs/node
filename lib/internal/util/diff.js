'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeSlice,
  StringPrototypeRepeat,
  StringPrototypeSplit,
} = primordials;

const { getLazy } = require('internal/util');
const { inspect } = require('internal/util/inspect');
const { myersDiff, printMyersDiff, printSimpleMyersDiff } = require('internal/assert/myers_diff');

const { kReadableOperator } = require('internal/assert/consts');
const colors = require('internal/util/colors');

function inspectValue(val) {
  // The util.inspect default values could be changed. This makes sure the
  // error messages contain the necessary information nevertheless.
  return inspect(val, {
    compact: false,
    customInspect: false,
    depth: 1000,
    maxArrayLength: Infinity,
    // We check only enumerable properties
    showHidden: false,
    showProxy: false,
    sorted: true,
    // Inspect getters as we also check them when comparing entries
    getters: true,
  });
}

function getColoredMyersDiff(actual, expected) {
  const header = `${colors.green}actual${colors.white} ${colors.red}expected${colors.white}`;
  const skipped = false;

  const diff = myersDiff(StringPrototypeSplit(actual, ''), StringPrototypeSplit(expected, ''));
  let message = printSimpleMyersDiff(diff);

  if (skipped) {
    message += '...';
  }

  return { message, header, skipped };
}

function getStackedDiff(actual, expected) {
  const isStringComparison = typeof actual === 'string' && typeof expected === 'string';

  let message = `\n${colors.green}+${colors.white} ${actual}\n${colors.red}- ${colors.white}${expected}`;
  const stringsLen = actual.length + expected.length;
  const maxTerminalLength = process.stderr.isTTY ? process.stderr.columns : 80;
  const showIndicator = isStringComparison && (stringsLen <= maxTerminalLength);

  if (showIndicator) {
    let indicatorIdx = -1;

    for (let i = 0; i < actual.length; i++) {
      if (actual[i] !== expected[i]) {
        // Skip the indicator for the first 2 characters because the diff is immediately apparent
        // It is 3 instead of 2 to account for the quotes
        if (i >= 3) {
          indicatorIdx = i;
        }
        break;
      }
    }

    if (indicatorIdx !== -1) {
      message += `\n${StringPrototypeRepeat(' ', indicatorIdx + 2)}^`;
    }
  }

  return { message };
}

function getSimpleDiff(originalActual, actual, originalExpected, expected) {
  const kMaxShortStringLength = 12;

  let stringsLen = actual.length + expected.length;
  // Accounting for the quotes wrapping strings
  if (typeof originalActual === 'string') {
    stringsLen -= 2;
  }
  if (typeof originalExpected === 'string') {
    stringsLen -= 2;
  }
  if (stringsLen <= kMaxShortStringLength && (originalActual !== 0 || originalExpected !== 0)) {
    return { message: `${actual} !== ${expected}`, header: '' };
  }

  const isStringComparison = typeof originalActual === 'string' && typeof originalExpected === 'string';

  if (isStringComparison && colors.hasColors) {
    return getColoredMyersDiff(actual, expected);
  }

  return getStackedDiff(actual, expected);
}

function isSimpleDiff(actual, inspectedActual, expected, inspectedExpected) {
  if (inspectedActual.length > 1 || inspectedExpected.length > 1) {
    return false;
  }

  return typeof actual !== 'object' || actual === null || typeof expected !== 'object' || expected === null;
}

function getErrorMessage(operator, message) {
  if (!operator && !message) {
    return '';
  }
  return message || kReadableOperator[operator];
}

/**
 * Generate a difference report between two values
 * @param {any} actual - The first value to compare
 * @param {any} expected - The second value to compare
 * @returns {string|null} - Formatted diff string or null if values are equal
 */
// The following parameters are not documented because they are only used internally
// @param {string} [operator] - The operator used in the assertion
// @param {string} [customMessage] - Custom message to display in the diff, still just in the assertion
function diff(actual, expected, operator, customMessage) {
  // The equality check should only be done when not called from the assertion module
  // When coming from there, we know for sure actual and expected are not equal
  if (!operator) {
    // First check if the values are deeply equal
    try {
      const assert = getLazy(() => require('assert'))();
      // eslint-disable-next-line
      assert.deepStrictEqual(actual, expected);
      // If we reach here, the values are equal, so return null
      return null;
    } catch {
      // Values are different, continue to generate diff
    }
  }

  let skipped = false;
  let message = '';
  const inspectedActual = inspectValue(actual);
  const inspectedExpected = inspectValue(expected);
  const inspectedSplitActual = StringPrototypeSplit(inspectedActual, '\n');
  const inspectedSplitExpected = StringPrototypeSplit(inspectedExpected, '\n');
  const showSimpleDiff = isSimpleDiff(actual, inspectedSplitActual, expected, inspectedSplitExpected);
  let header = `${colors.green}+ actual${colors.white} ${colors.red}- expected${colors.white}`;

  if (showSimpleDiff) {
    const simpleDiff = getSimpleDiff(actual, inspectedSplitActual[0], expected, inspectedSplitExpected[0]);
    message = simpleDiff.message;
    if (typeof simpleDiff.header !== 'undefined') {
      header = simpleDiff.header;
    }
    if (simpleDiff.skipped) {
      skipped = true;
    }
  } else if (inspectedActual === inspectedExpected) {
    // Handles the case where the objects are structurally the same but different references
    if (inspectedSplitActual.length > 50) {
      message = `${ArrayPrototypeJoin(ArrayPrototypeSlice(inspectedSplitActual, 0, 50), '\n')}\n...}`;
      skipped = true;
    } else {
      message = ArrayPrototypeJoin(inspectedSplitActual, '\n');
    }
    if (operator) {
      operator = 'notIdentical';
      header = '';
    } else {
      header = `${kReadableOperator.notIdentical}\n`;
    }
  } else {
    const checkCommaDisparity = actual != null && typeof actual === 'object';
    const diff = myersDiff(inspectedSplitActual, inspectedSplitExpected, checkCommaDisparity);

    const myersDiffMessage = printMyersDiff(diff, operator);
    message = myersDiffMessage.message;

    if (operator === 'partialDeepStrictEqual') {
      header = `${colors.gray}${colors.hasColors ? '' : '+ '}actual${colors.white} ${colors.red}- expected${colors.white}`;
    }

    if (myersDiffMessage.skipped) {
      skipped = true;
    }
  }

  const errorMessage = getErrorMessage(operator, customMessage);
  const headerMessage = errorMessage ? `${errorMessage}\n${header}` : header;
  const skippedMessage = skipped ? '\n... Skipped lines' : '';

  return `${headerMessage}${skippedMessage}\n${message}\n`;
}

module.exports = {
  inspectValue,
  getColoredMyersDiff,
  getStackedDiff,
  getSimpleDiff,
  isSimpleDiff,
  diff,
};
