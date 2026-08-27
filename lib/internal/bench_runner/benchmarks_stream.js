'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  Symbol,
} = primordials;
const Readable = require('internal/streams/readable');

const kEmitMessage = Symbol('kEmitMessage');
const kBenchmarksStreamDrain = Symbol('kBenchmarksStreamDrain');

class BenchmarksStream extends Readable {
  #buffer = [];
  #canPush = true;

  constructor() {
    super({
      __proto__: null,
      objectMode: true,
    });
  }

  _read() {
    const wasBlocked = !this.#canPush;
    this.#canPush = true;
    while (this.#buffer.length > 0) {
      const record = ArrayPrototypeShift(this.#buffer);
      if (!this.#tryPush(record)) return;
    }
    if (wasBlocked) this.emit(kBenchmarksStreamDrain);
  }

  start(data) {
    return this[kEmitMessage]('bench:start', data);
  }

  sample(data) {
    return this[kEmitMessage]('bench:sample', data);
  }

  complete(data) {
    return this[kEmitMessage]('bench:complete', data);
  }

  diagnostic(data) {
    return this[kEmitMessage]('bench:diagnostic', data);
  }

  summary(data) {
    return this[kEmitMessage]('bench:summary', data);
  }

  end() {
    return this.#tryPush(null);
  }

  [kEmitMessage](type, data) {
    this.emit(type, data);
    return this.#tryPush({ type, data });
  }

  #tryPush(record) {
    if (this.#canPush) {
      this.#canPush = this.push(record);
    } else {
      ArrayPrototypePush(this.#buffer, record);
    }
    return this.#canPush;
  }
}

module.exports = {
  BenchmarksStream,
  kBenchmarksStreamDrain,
};
