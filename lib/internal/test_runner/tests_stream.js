'use strict';
const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
} = primordials;
const Readable = require('internal/streams/readable');

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
    this.#emit('test:fail', { __proto__: null, name, nesting, file, testNumber, details, ...directive });
  }

  ok(nesting, file, testNumber, name, details, directive) {
    this.#emit('test:pass', { __proto__: null, name, nesting, file, testNumber, details, ...directive });
  }

  plan(nesting, file, count) {
    this.#emit('test:plan', { __proto__: null, nesting, file, count });
  }

  getSkip(reason = undefined) {
    return { __proto__: null, skip: reason ?? true };
  }

  getTodo(reason = undefined) {
    return { __proto__: null, todo: reason ?? true };
  }

  start(nesting, file, name) {
    this.#emit('test:start', { __proto__: null, nesting, file, name });
  }

  diagnostic(nesting, file, message) {
    this.#emit('test:diagnostic', { __proto__: null, nesting, file, message });
  }

  coverage(nesting, file, summary) {
    this.#emit('test:coverage', { __proto__: null, nesting, file, summary });
  }

  #emit(type, data) {
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

module.exports = { TestsStream };
