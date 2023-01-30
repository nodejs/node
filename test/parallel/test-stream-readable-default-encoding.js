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

  const chunks = [];
  r.on('data', (chunk) => chunks.push(chunk));

  process.nextTick(common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString('hex'), 'ab');
  }), 1);

}

{
  const r = new Readable({
    read() {},
    defaultEncoding: 'hex',
  });

  r.push('ab', 'utf-8');

  const chunks = [];
  r.on('data', (chunk) => chunks.push(chunk));

  process.nextTick(common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString('utf-8'), 'ab');
  }), 1);
}
