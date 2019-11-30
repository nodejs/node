'use strict';

const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');

// basic
{
  // Find it on Writable.prototype
  assert(Writable.prototype.hasOwnProperty('writableFinished'));
}

// event
{
  const writable = new Writable();

  writable._write = (chunk, encoding, cb) => {
    // The state finished should start in false.
    assert.strictEqual(writable.writableFinished, false);
    cb();
  };

  writable.on('finish', common.mustCall(() => {
    assert.strictEqual(writable.writableFinished, true);
  }));

  writable.end('testing finished state', common.mustCall(() => {
    assert.strictEqual(writable.writableFinished, true);
  }));
}

{
  // Emit finish asynchronously

  const w = new Writable({
    write(chunk, encoding, cb) {
      cb();
    }
  });

  w.end();
  w.on('finish', common.mustCall());
}
