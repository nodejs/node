'use strict';
const {
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ObjectEntries,
  StringPrototypeReplaceAll,
  StringPrototypeSplit,
  RegExpPrototypeSymbolReplace,
} = primordials;
const { inspectWithNoCustomRetry } = require('internal/errors');
const Readable = require('internal/streams/readable');
const { isError } = require('internal/util');
const kFrameStartRegExp = /^ {4}at /;
const kLineBreakRegExp = /\n|\r\n/;
const inspectOptions = { colors: false, breakLength: Infinity };
let testModule; // Lazy loaded due to circular dependency.

function lazyLoadTest() {
  testModule ??= require('internal/test_runner/test');

  return testModule;
}

class TapStream extends Readable {
  #buffer;
  #canPush;

  constructor() {
    super();
    this.#buffer = [];
    this.#canPush = true;
  }

  _read() {
    this.#canPush = true;

    while (this.#buffer.length > 0) {
      const line = ArrayPrototypeShift(this.#buffer);

      if (!this.#tryPush(line)) {
        return;
      }
    }
  }

  bail(message) {
    this.#tryPush(`Bail out!${message ? ` ${tapEscape(message)}` : ''}\n`);
  }

  fail(indent, testNumber, description, directive) {
    this.#test(indent, testNumber, 'not ok', description, directive);
  }

  ok(indent, testNumber, description, directive) {
    this.#test(indent, testNumber, 'ok', description, directive);
  }

  plan(indent, count, explanation) {
    const exp = `${explanation ? ` # ${tapEscape(explanation)}` : ''}`;

    this.#tryPush(`${indent}1..${count}${exp}\n`);
  }

  getSkip(reason) {
    return `SKIP${reason ? ` ${tapEscape(reason)}` : ''}`;
  }

  getTodo(reason) {
    return `TODO${reason ? ` ${tapEscape(reason)}` : ''}`;
  }

  details(indent, duration, error) {
    let details = `${indent}  ---\n`;

    details += jsToYaml(indent, 'duration_ms', duration);
    details += jsToYaml(indent, null, error);
    details += `${indent}  ...\n`;
    this.#tryPush(details);
  }

  diagnostic(indent, message) {
    this.#tryPush(`${indent}# ${tapEscape(message)}\n`);
  }

  version() {
    this.#tryPush('TAP version 13\n');
  }

  #test(indent, testNumber, status, description, directive) {
    let line = `${indent}${status} ${testNumber}`;

    if (description) {
      line += ` ${tapEscape(description)}`;
    }

    if (directive) {
      line += ` # ${directive}`;
    }

    line += '\n';
    this.#tryPush(line);
  }

  #tryPush(message) {
    if (this.#canPush) {
      this.#canPush = this.push(message);
    } else {
      ArrayPrototypePush(this.#buffer, message);
    }

    return this.#canPush;
  }
}

// In certain places, # and \ need to be escaped as \# and \\.
function tapEscape(input) {
  return StringPrototypeReplaceAll(
    StringPrototypeReplaceAll(input, '\\', '\\\\'), '#', '\\#'
  );
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
    const { kTestCodeFailure } = lazyLoadTest();
    const {
      cause,
      code,
      failureType,
      message,
      stack,
    } = value;
    let errMsg = message ?? '<unknown error>';
    let errStack = stack;
    let errCode = code;

    // If the ERR_TEST_FAILURE came from an error provided by user code,
    // then try to unwrap the original error message and stack.
    if (code === 'ERR_TEST_FAILURE' && failureType === kTestCodeFailure) {
      errMsg = cause?.message ?? errMsg;
      errStack = cause?.stack ?? errStack;
      errCode = cause?.code ?? errCode;
    }

    result += jsToYaml(indent, 'error', errMsg);

    if (errCode) {
      result += jsToYaml(indent, 'code', errCode);
    }

    if (typeof errStack === 'string') {
      const frames = [];

      ArrayPrototypeForEach(
        StringPrototypeSplit(errStack, kLineBreakRegExp),
        (frame) => {
          const processed = RegExpPrototypeSymbolReplace(
            kFrameStartRegExp, frame, ''
          );

          if (processed.length > 0 && processed.length !== frame.length) {
            ArrayPrototypePush(frames, processed);
          }
        }
      );

      if (frames.length > 0) {
        const frameDelimiter = `\n${indent}    `;

        result += `${indent}  stack: |-${frameDelimiter}`;
        result += `${ArrayPrototypeJoin(frames, `${frameDelimiter}`)}\n`;
      }
    }
  }

  return result;
}

module.exports = { TapStream };
