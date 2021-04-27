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

class Interface extends _Interface {
  // eslint-disable-next-line no-useless-constructor
  constructor(input, output, completer, terminal) {
    super(input, output, completer, terminal);
  }
  question(query, options = {}) {
    return new Promise((resolve, reject) => {
      if (options.signal) {
        if (options.signal.aborted) {
          return reject(new AbortError());
        }

        options.signal.addEventListener('abort', () => {
          this[kQuestionCancel]();
          reject(new AbortError());
        }, { once: true });
      }

      super.question(query, resolve);
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
