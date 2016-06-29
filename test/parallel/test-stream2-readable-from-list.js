// Flags: --expose_internals
'use strict';
require('../common');
var assert = require('assert');
var fromList = require('_stream_readable')._fromList;
var BufferList = require('internal/streams/BufferList');

// tiny node-tap lookalike.
var tests = [];
var count = 0;

function test(name, fn) {
  count++;
  tests.push([name, fn]);
}

function run() {
  var next = tests.shift();
  if (!next)
    return console.error('ok');

  var name = next[0];
  var fn = next[1];
  console.log('# %s', name);
  fn({
    same: assert.deepEqual,
    equal: assert.equal,
    end: function() {
      count--;
      run();
    }
  });
}

function bufferListFromArray(arr) {
  const bl = new BufferList();
  for (var i = 0; i < arr.length; ++i)
    bl.push(arr[i]);
  return bl;
}

// ensure all tests have run
process.on('exit', function() {
  assert.equal(count, 0);
});

process.nextTick(run);


test('buffers', function(t) {
  var list = [ Buffer.from('foog'),
               Buffer.from('bark'),
               Buffer.from('bazy'),
               Buffer.from('kuel') ];
  list = bufferListFromArray(list);

  // read more than the first element.
  var ret = fromList(6, { buffer: list, length: 16 });
  t.equal(ret.toString(), 'foogba');

  // read exactly the first element.
  ret = fromList(2, { buffer: list, length: 10 });
  t.equal(ret.toString(), 'rk');

  // read less than the first element.
  ret = fromList(2, { buffer: list, length: 8 });
  t.equal(ret.toString(), 'ba');

  // read more than we have.
  ret = fromList(100, { buffer: list, length: 6 });
  t.equal(ret.toString(), 'zykuel');

  // all consumed.
  t.same(list, new BufferList());

  t.end();
});

test('strings', function(t) {
  var list = [ 'foog',
               'bark',
               'bazy',
               'kuel' ];
  list = bufferListFromArray(list);

  // read more than the first element.
  var ret = fromList(6, { buffer: list, length: 16, decoder: true });
  t.equal(ret, 'foogba');

  // read exactly the first element.
  ret = fromList(2, { buffer: list, length: 10, decoder: true });
  t.equal(ret, 'rk');

  // read less than the first element.
  ret = fromList(2, { buffer: list, length: 8, decoder: true });
  t.equal(ret, 'ba');

  // read more than we have.
  ret = fromList(100, { buffer: list, length: 6, decoder: true });
  t.equal(ret, 'zykuel');

  // all consumed.
  t.same(list, new BufferList());

  t.end();
});
