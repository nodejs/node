'use strict';

const { inspect } = require('util');
const { codes: {
  ERR_INVALID_ARG_TYPE
} } = require('internal/errors');

let blue = '';
let green = '';
let red = '';
let white = '';

const READABLE_OPERATOR = {
  deepStrictEqual: 'Input A expected to strictly deep-equal input B',
  notDeepStrictEqual: 'Input A expected to strictly not deep-equal input B',
  strictEqual: 'Input A expected to strictly equal input B',
  notStrictEqual: 'Input A expected to strictly not equal input B'
};

function copyError(source) {
  const keys = Object.keys(source);
  const target = Object.create(Object.getPrototypeOf(source));
  for (const key of keys) {
    target[key] = source[key];
  }
  Object.defineProperty(target, 'message', { value: source.message });
  return target;
}

function inspectValue(val) {
  // The util.inspect default values could be changed. This makes sure the
  // error messages contain the necessary information nevertheless.
  return inspect(
    val,
    {
      compact: false,
      customInspect: false,
      depth: 1000,
      maxArrayLength: Infinity,
      // Assert compares only enumerable properties (with a few exceptions).
      showHidden: false,
      // Having a long line as error is better than wrapping the line for
      // comparison.
      breakLength: Infinity,
      // Assert does not detect proxies currently.
      showProxy: false
    }
  ).split('\n');
}

function createErrDiff(actual, expected, operator) {
  var other = '';
  var res = '';
  var lastPos = 0;
  var end = '';
  var skipped = false;
  const actualLines = inspectValue(actual);
  const expectedLines = inspectValue(expected);
  const msg = READABLE_OPERATOR[operator] +
        `:\n${green}+ expected${white} ${red}- actual${white}`;
  const skippedMsg = ` ${blue}...${white} Lines skipped`;

  // Remove all ending lines that match (this optimizes the output for
  // readability by reducing the number of total changed lines).
  var a = actualLines[actualLines.length - 1];
  var b = expectedLines[expectedLines.length - 1];
  var i = 0;
  while (a === b) {
    if (i++ < 2) {
      end = `\n  ${a}${end}`;
    } else {
      other = a;
    }
    actualLines.pop();
    expectedLines.pop();
    if (actualLines.length === 0 || expectedLines.length === 0)
      break;
    a = actualLines[actualLines.length - 1];
    b = expectedLines[expectedLines.length - 1];
  }
  if (i > 3) {
    end = `\n${blue}...${white}${end}`;
    skipped = true;
  }
  if (other !== '') {
    end = `\n  ${other}${end}`;
    other = '';
  }

  const maxLines = Math.max(actualLines.length, expectedLines.length);
  var printedLines = 0;
  var identical = 0;
  for (i = 0; i < maxLines; i++) {
    // Only extra expected lines exist
    const cur = i - lastPos;
    if (actualLines.length < i + 1) {
      if (cur > 1 && i > 2) {
        if (cur > 4) {
          res += `\n${blue}...${white}`;
          skipped = true;
        } else if (cur > 3) {
          res += `\n  ${expectedLines[i - 2]}`;
          printedLines++;
        }
        res += `\n  ${expectedLines[i - 1]}`;
        printedLines++;
      }
      lastPos = i;
      other += `\n${green}+${white} ${expectedLines[i]}`;
      printedLines++;
    // Only extra actual lines exist
    } else if (expectedLines.length < i + 1) {
      if (cur > 1 && i > 2) {
        if (cur > 4) {
          res += `\n${blue}...${white}`;
          skipped = true;
        } else if (cur > 3) {
          res += `\n  ${actualLines[i - 2]}`;
          printedLines++;
        }
        res += `\n  ${actualLines[i - 1]}`;
        printedLines++;
      }
      lastPos = i;
      res += `\n${red}-${white} ${actualLines[i]}`;
      printedLines++;
    // Lines diverge
    } else if (actualLines[i] !== expectedLines[i]) {
      if (cur > 1 && i > 2) {
        if (cur > 4) {
          res += `\n${blue}...${white}`;
          skipped = true;
        } else if (cur > 3) {
          res += `\n  ${actualLines[i - 2]}`;
          printedLines++;
        }
        res += `\n  ${actualLines[i - 1]}`;
        printedLines++;
      }
      lastPos = i;
      res += `\n${red}-${white} ${actualLines[i]}`;
      other += `\n${green}+${white} ${expectedLines[i]}`;
      printedLines += 2;
    // Lines are identical
    } else {
      res += other;
      other = '';
      if (cur === 1 || i === 0) {
        res += `\n  ${actualLines[i]}`;
        printedLines++;
      }
      identical++;
    }
    // Inspected object to big (Show ~20 rows max)
    if (printedLines > 20 && i < maxLines - 2) {
      return `${msg}${skippedMsg}\n${res}\n${blue}...${white}${other}\n` +
        `${blue}...${white}`;
    }
  }

  // Strict equal with identical objects that are not identical by reference.
  if (identical === maxLines) {
    // E.g., assert.deepStrictEqual(Symbol(), Symbol())
    const base = operator === 'strictEqual' ?
      'Input objects identical but not reference equal:' :
      'Input objects not identical:';

    // We have to get the result again. The lines were all removed before.
    const actualLines = inspectValue(actual);

    // Only remove lines in case it makes sense to collapse those.
    // TODO: Accept env to always show the full error.
    if (actualLines.length > 30) {
      actualLines[26] = `${blue}...${white}`;
      while (actualLines.length > 27) {
        actualLines.pop();
      }
    }

    return `${base}\n\n${actualLines.join('\n')}\n`;
  }
  return `${msg}${skipped ? skippedMsg : ''}\n${res}${other}${end}`;
}

class AssertionError extends Error {
  constructor(options) {
    if (typeof options !== 'object' || options === null) {
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
    }
    var {
      actual,
      expected,
      message,
      operator,
      stackStartFn
    } = options;

    if (message != null) {
      super(String(message));
    } else {
      if (process.stdout.isTTY) {
        // Reset on each call to make sure we handle dynamically set environment
        // variables correct.
        if (process.stdout.getColorDepth() !== 1) {
          blue = '\u001b[34m';
          green = '\u001b[32m';
          white = '\u001b[39m';
          red = '\u001b[31m';
        } else {
          blue = '';
          green = '';
          white = '';
          red = '';
        }
      }
      // Prevent the error stack from being visible by duplicating the error
      // in a very close way to the original in case both sides are actually
      // instances of Error.
      if (typeof actual === 'object' && actual !== null &&
          typeof expected === 'object' && expected !== null &&
          'stack' in actual && actual instanceof Error &&
          'stack' in expected && expected instanceof Error) {
        actual = copyError(actual);
        expected = copyError(expected);
      }

      if (operator === 'deepStrictEqual' || operator === 'strictEqual') {
        super(createErrDiff(actual, expected, operator));
      } else if (operator === 'notDeepStrictEqual' ||
        operator === 'notStrictEqual') {
        // In case the objects are equal but the operator requires unequal, show
        // the first object and say A equals B
        const res = inspectValue(actual);
        const base = `Identical input passed to ${operator}:`;

        // Only remove lines in case it makes sense to collapse those.
        // TODO: Accept env to always show the full error.
        if (res.length > 30) {
          res[26] = `${blue}...${white}`;
          while (res.length > 27) {
            res.pop();
          }
        }

        // Only print a single input.
        if (res.length === 1) {
          super(`${base} ${res[0]}`);
        } else {
          super(`${base}\n\n${res.join('\n')}\n`);
        }
      } else {
        let res = inspect(actual);
        let other = inspect(expected);
        if (res.length > 128)
          res = `${res.slice(0, 125)}...`;
        if (other.length > 128)
          other = `${other.slice(0, 125)}...`;
        super(`${res} ${operator} ${other}`);
      }
    }

    this.generatedMessage = !message;
    this.name = 'AssertionError [ERR_ASSERTION]';
    this.code = 'ERR_ASSERTION';
    this.actual = actual;
    this.expected = expected;
    this.operator = operator;
    Error.captureStackTrace(this, stackStartFn);
  }
}

module.exports = {
  AssertionError,
  errorCache: new Map()
};
