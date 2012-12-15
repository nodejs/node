// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var assert = require('assert');
var common = require('../common.js');
var fromList = require('_stream_readable')._fromList;

// tiny node-tap lookalike.
var tests = [];
function test(name, fn) {
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
    end: run
  });
}

process.nextTick(run);



test('buffers', function(t) {
  // have a length
  var len = 16;
  var list = [ new Buffer('foog'),
               new Buffer('bark'),
               new Buffer('bazy'),
               new Buffer('kuel') ];

  // read more than the first element.
  var ret = fromList(6, list, 16);
  t.equal(ret.toString(), 'foogba');

  // read exactly the first element.
  ret = fromList(2, list, 10);
  t.equal(ret.toString(), 'rk');

  // read less than the first element.
  ret = fromList(2, list, 8);
  t.equal(ret.toString(), 'ba');

  // read more than we have.
  ret = fromList(100, list, 6);
  t.equal(ret.toString(), 'zykuel');

  // all consumed.
  t.same(list, []);

  t.end();
});

test('strings', function(t) {
  // have a length
  var len = 16;
  var list = [ 'foog',
               'bark',
               'bazy',
               'kuel' ];

  // read more than the first element.
  var ret = fromList(6, list, 16, true);
  t.equal(ret, 'foogba');

  // read exactly the first element.
  ret = fromList(2, list, 10, true);
  t.equal(ret, 'rk');

  // read less than the first element.
  ret = fromList(2, list, 8, true);
  t.equal(ret, 'ba');

  // read more than we have.
  ret = fromList(100, list, 6, true);
  t.equal(ret, 'zykuel');

  // all consumed.
  t.same(list, []);

  t.end();
});
