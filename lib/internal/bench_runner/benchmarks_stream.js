'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  NumberMAX_SAFE_INTEGER,
  Symbol,
} = primordials;
const Readable = require('internal/streams/readable');

const kEmitMessage = Symbol('kEmitMessage');

class BenchmarksStream extends Readable {
  #buffer = [];
  #canPush = true;

  constructor() {
    super({
      __proto__: null,
      objectMode: true,
      highWaterMark: NumberMAX_SAFE_INTEGER,
    });
  }

  _read() {
    this.#canPush = true;
    while (this.#buffer.length > 0) {
      const record = ArrayPrototypeShift(this.#buffer);
      if (!this.#tryPush(record)) return;
    }
  }

  start(data) {
    this[kEmitMessage]('bench:start', data);
  }

  sample(data) {
    this[kEmitMessage]('bench:sample', data);
  }

  complete(data) {
    this[kEmitMessage]('bench:complete', data);
  }

  diagnostic(data) {
    this[kEmitMessage]('bench:diagnostic', data);
  }

  summary(data) {
    this[kEmitMessage]('bench:summary', data);
  }

  end() {
    this.#tryPush(null);
  }

  [kEmitMessage](type, data) {
    this.emit(type, data);
    this.#tryPush({ type, data });
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
};
