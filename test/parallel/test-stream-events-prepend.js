'use strict';
const common = require('../common');
const stream = require('stream');

class Writable extends stream.Writable {
  constructor() {
    super();
    this.prependListener = undefined;
  }

  _write(chunk, end, cb) {
    cb();
  }
}

class Readable extends stream.Readable {
  _read() {
    this.push(null);
  }
}

// Expect deprecation warning when using fallback path
common.expectWarning(
  'DeprecationWarning',
  'Piping to an EventEmitter without a prependListener method is deprecated. ' +
  'The emitter should have a prependListener method.',
  'DEP0206',
);

const w = new Writable();
w.on('pipe', common.mustCall());

const r = new Readable();
r.pipe(w);
