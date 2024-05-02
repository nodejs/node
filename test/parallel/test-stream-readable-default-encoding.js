'use strict';
const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

{
  assert.throws(() => {
    new Readable({
      read: () => {},
      defaultEncoding: 'my invalid encoding',
    });
  }, {
    code: 'ERR_UNKNOWN_ENCODING',
  });
}

{
  const r = new Readable({
    read() {},
    defaultEncoding: 'hex'
  });

  r.push('ab');

  r.on('data', common.mustCall((chunk) => assert.strictEqual(chunk.toString('hex'), 'ab')), 1);
}

{
  const r = new Readable({
    read() {},
    defaultEncoding: 'hex',
  });

  r.push('xy', 'utf-8');

  r.on('data', common.mustCall((chunk) => assert.strictEqual(chunk.toString('utf-8'), 'xy')), 1);
}
