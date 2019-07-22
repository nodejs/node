'use strict';

const common = require('../common');
const { Duplex } = require('stream');
const assert = require('assert');

// basic
{
  // Find it on Duplex.prototype
  assert(Duplex.prototype.hasOwnProperty('writableFinished'));
}

// event
{
  const duplex = new Duplex();

  duplex._write = (chunk, encoding, cb) => {
    // The state finished should start in false.
    assert.strictEqual(duplex.writableFinished, false);
    cb();
  };

  duplex.on('finish', common.mustCall(() => {
    assert.strictEqual(duplex.writableFinished, true);
  }));

  duplex.end('testing finished state', common.mustCall(() => {
    assert.strictEqual(duplex.writableFinished, true);
  }));
}
