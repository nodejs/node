'use strict';

const {
  Promise,
} = primordials;

const {
  Readline,
} = require('internal/readline/promises');

const {
  Interface: _Interface,
  kQuestionCancel,
} = require('internal/readline/interface');

const {
  AbortError,
} = require('internal/errors');
const { validateAbortSignal } = require('internal/validators');

const {
  kEmptyObject,
} = require('internal/util');

class Interface extends _Interface {
  // eslint-disable-next-line no-useless-constructor
  constructor(input, output, completer, terminal) {
    super(input, output, completer, terminal);
  }
  question(query, options = kEmptyObject) {
    return new Promise((resolve, reject) => {
      let cb = resolve;

      if (options?.signal) {
        validateAbortSignal(options.signal, 'options.signal');
        if (options.signal.aborted) {
          return reject(
            new AbortError(undefined, { cause: options.signal.reason }));
        }

        const onAbort = () => {
          this[kQuestionCancel]();
          reject(new AbortError(undefined, { cause: options.signal.reason }));
        };
        options.signal.addEventListener('abort', onAbort, { once: true });
        cb = (answer) => {
          options.signal.removeEventListener('abort', onAbort);
          resolve(answer);
        };
      }

      super.question(query, cb);
    });
  }
}

function createInterface(input, output, completer, terminal) {
  return new Interface(input, output, completer, terminal);
}

module.exports = {
  Interface,
  Readline,
  createInterface,
};
