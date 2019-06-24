'use strict';
const { Object } = primordials;
const EventEmitter = require('events');
const {
  Interface: CallbackInterface,
  initializeInterface,
  onTabComplete,
  normalizeInterfaceArguments
} = require('internal/readline/interface');


class Interface extends EventEmitter {
  constructor(input, output, completer, terminal) {
    super();
    const opts = normalizeInterfaceArguments(
      input, output, completer, terminal);
    initializeInterface(this, opts);
  }

  question(query) {
    let resolve;
    const promise = new Promise((res) => {
      resolve = res;
    });

    if (!this._questionCallback) {
      this._oldPrompt = this._prompt;
      this.setPrompt(query);
      this._questionCallback = resolve;
    }

    this.prompt();
    return promise;
  }

  async _tabComplete(lastKeypressWasTab) {
    this.pause();

    try {
      const line = this.line.slice(0, this.cursor);
      const results = await this.completer(line);
      onTabComplete(null, results, this, lastKeypressWasTab);
    } catch (err) {
      onTabComplete(err, null, this, lastKeypressWasTab);
    }
  }
}

// Copy the rest of the callback interface over to this interface.
Object.keys(CallbackInterface.prototype).forEach((keyName) => {
  if (Interface.prototype[keyName] === undefined)
    Interface.prototype[keyName] = CallbackInterface.prototype[keyName];
});

Object.defineProperty(Interface.prototype, 'columns', {
  configurable: true,
  enumerable: true,
  get() {
    return this.output && this.output.columns ? this.output.columns : Infinity;
  }
});

Interface.prototype[Symbol.asyncIterator] =
    CallbackInterface.prototype[Symbol.asyncIterator];


function createInterface(input, output, completer, terminal) {
  return new Interface(input, output, completer, terminal);
}


module.exports = { createInterface, Interface };
