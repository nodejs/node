'use strict';
require('../common');
const { PassThrough } = require('stream');
const assert = require('assert');

{
  const stream = new PassThrough();
  stream.setEncoding('utf8');
  stream.write('foobar');
  assert.strictEqual(stream.read(), 'foobar');
  stream.setEncoding(null);
  stream.write('foobar');
  assert.deepStrictEqual(stream.read(), Buffer.from('foobar'));
}

{
  const stream = new PassThrough();
  stream.setEncoding('utf8');
  stream.write('foobar');
  assert.strictEqual(stream.read(), 'foobar');
  stream.setEncoding('buffer');
  stream.write('foobar');
  assert.deepStrictEqual(stream.read(), Buffer.from('foobar'));
}

{
  const stream = new PassThrough();
  stream.setEncoding('utf8');
  stream.write(Buffer.from([0xc3]));
  stream.setEncoding('buffer');
  assert.strictEqual(stream.read(), '\ufffd');
}
