// Flags: --expose_internals
'use strict';
require('../common');
const assert = require('assert');
const fromList = require('_stream_readable')._fromList;
const BufferList = require('internal/streams/BufferList');

function bufferListFromArray(arr) {
  const bl = new BufferList();
  for (let i = 0; i < arr.length; ++i)
    bl.push(arr[i]);
  return bl;
}

{
  // Verify behavior with buffers
  let list = [ Buffer.from('foog'),
               Buffer.from('bark'),
               Buffer.from('bazy'),
               Buffer.from('kuel') ];
  list = bufferListFromArray(list);

  // read more than the first element.
  let ret = fromList(6, { buffer: list, length: 16 });
  assert.strictEqual(ret.toString(), 'foogba');

  // read exactly the first element.
  ret = fromList(2, { buffer: list, length: 10 });
  assert.strictEqual(ret.toString(), 'rk');

  // read less than the first element.
  ret = fromList(2, { buffer: list, length: 8 });
  assert.strictEqual(ret.toString(), 'ba');

  // read more than we have.
  ret = fromList(100, { buffer: list, length: 6 });
  assert.strictEqual(ret.toString(), 'zykuel');

  // all consumed.
  assert.deepStrictEqual(list, new BufferList());
}

{
  // Verify behavior with strings
  let list = [ 'foog',
               'bark',
               'bazy',
               'kuel' ];
  list = bufferListFromArray(list);

  // read more than the first element.
  let ret = fromList(6, { buffer: list, length: 16, decoder: true });
  assert.strictEqual(ret, 'foogba');

  // read exactly the first element.
  ret = fromList(2, { buffer: list, length: 10, decoder: true });
  assert.strictEqual(ret, 'rk');

  // read less than the first element.
  ret = fromList(2, { buffer: list, length: 8, decoder: true });
  assert.strictEqual(ret, 'ba');

  // read more than we have.
  ret = fromList(100, { buffer: list, length: 6, decoder: true });
  assert.strictEqual(ret, 'zykuel');

  // all consumed.
  assert.deepStrictEqual(list, new BufferList());
}
