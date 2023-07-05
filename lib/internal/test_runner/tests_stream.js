'use strict';
const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  Symbol,
} = primordials;
const Readable = require('internal/streams/readable');

const kEmitMessage = Symbol('kEmitMessage');
class TestsStream extends Readable {
  #buffer;
  #canPush;

  constructor() {
    super({ objectMode: true });
    this.#buffer = [];
    this.#canPush = true;
  }

  _read() {
    this.#canPush = true;

    while (this.#buffer.length > 0) {
      const obj = ArrayPrototypeShift(this.#buffer);

      if (!this.#tryPush(obj)) {
        return;
      }
    }
  }

  fail(nesting, file, testNumber, name, details, directive) {
    this[kEmitMessage]('test:fail', { __proto__: null, name, nesting, file, testNumber, details, ...directive });
  }

  ok(nesting, file, testNumber, name, details, directive) {
    this[kEmitMessage]('test:pass', { __proto__: null, name, nesting, file, testNumber, details, ...directive });
  }

  plan(nesting, file, count) {
    this[kEmitMessage]('test:plan', { __proto__: null, nesting, file, count });
  }

  getSkip(reason = undefined) {
    return { __proto__: null, skip: reason ?? true };
  }

  getTodo(reason = undefined) {
    return { __proto__: null, todo: reason ?? true };
  }

  enqueue(nesting, file, name) {
    this[kEmitMessage]('test:enqueue', { __proto__: null, nesting, file, name });
  }

  dequeue(nesting, file, name) {
    this[kEmitMessage]('test:dequeue', { __proto__: null, nesting, file, name });
  }

  start(nesting, file, name) {
    this[kEmitMessage]('test:start', { __proto__: null, nesting, file, name });
  }

  diagnostic(nesting, file, message) {
    this[kEmitMessage]('test:diagnostic', { __proto__: null, nesting, file, message });
  }

  stderr(file, message) {
    this[kEmitMessage]('test:stderr', { __proto__: null, file, message });
  }

  stdout(file, message) {
    this[kEmitMessage]('test:stdout', { __proto__: null, file, message });
  }

  coverage(nesting, file, summary) {
    this[kEmitMessage]('test:coverage', { __proto__: null, nesting, file, summary });
  }

  end() {
    this.#tryPush(null);
  }

  [kEmitMessage](type, data) {
    this.emit(type, data);
    this.#tryPush({ type, data });
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

module.exports = { TestsStream, kEmitMessage };
