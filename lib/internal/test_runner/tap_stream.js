'use strict';
const {
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ObjectEntries,
  StringPrototypeReplace,
  StringPrototypeReplaceAll,
  StringPrototypeSplit,
} = primordials;
const Readable = require('internal/streams/readable');
const { isError } = require('internal/util');
const { inspect } = require('internal/util/inspect');
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

    details += `${indent}  duration_ms: ${duration}\n`;

    if (error !== null && typeof error === 'object') {
      const entries = ObjectEntries(error);
      const isErrorObj = isError(error);

      for (let i = 0; i < entries.length; i++) {
        const { 0: key, 1: value } = entries[i];

        if (isError && (key === 'cause' || key === 'code')) {
          continue;
        }

        details += `${indent}  ${key}: ${inspect(value, inspectOptions)}\n`;
      }

      if (isErrorObj) {
        const { kTestCodeFailure } = lazyLoadTest();
        const {
          cause,
          code,
          failureType,
          message,
          stack,
        } = error;
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

        details += `${indent}  error: ${inspect(errMsg, inspectOptions)}\n`;

        if (errCode) {
          details += `${indent}  code: ${errCode}\n`;
        }

        if (typeof errStack === 'string') {
          const frames = [];

          ArrayPrototypeForEach(
            StringPrototypeSplit(errStack, kLineBreakRegExp),
            (frame) => {
              const processed = StringPrototypeReplace(
                frame, kFrameStartRegExp, ''
              );

              if (processed.length > 0 && processed.length !== frame.length) {
                ArrayPrototypePush(frames, processed);
              }
            }
          );

          if (frames.length > 0) {
            const frameDelimiter = `\n${indent}    `;

            details += `${indent}  stack: |-${frameDelimiter}`;
            details += `${ArrayPrototypeJoin(frames, `${frameDelimiter}`)}\n`;
          }
        }
      }
    } else if (error !== null && error !== undefined) {
      details += `${indent}  error: ${inspect(error, inspectOptions)}\n`;
    }

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

module.exports = { TapStream };
