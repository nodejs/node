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


var common = require('../common.js');
var assert = require('assert');
var R = require('_stream_readable');
var util = require('util');

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
    end: function () {
      count--;
      run();
    }
  });
}

// ensure all tests have run
process.on("exit", function () {
  assert.equal(count, 0);
});

process.nextTick(run);

/////

util.inherits(TestReader, R);

function TestReader(n, opts) {
  R.call(this, opts);

  this.pos = 0;
  this.len = n || 100;
}

TestReader.prototype._read = function(n) {
  setTimeout(function() {

    if (this.pos >= this.len) {
      // double push(null) to test eos handling
      this.push(null);
      return this.push(null);
    }

    n = Math.min(n, this.len - this.pos);
    if (n <= 0) {
      // double push(null) to test eos handling
      this.push(null);
      return this.push(null);
    }

    this.pos += n;
    var ret = new Buffer(n);
    ret.fill('a');

    console.log("this.push(ret)", ret)

    return this.push(ret);
  }.bind(this), 1);
};

test('setEncoding utf8', function(t) {
  var tr = new TestReader(100);
  tr.setEncoding('utf8');
  var out = [];
  var expect =
    [ 'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa' ];

  tr.on('readable', function flow() {
    var chunk;
    while (null !== (chunk = tr.read(10)))
      out.push(chunk);
  });

  tr.on('end', function() {
    t.same(out, expect);
    t.end();
  });
});


test('setEncoding hex', function(t) {
  var tr = new TestReader(100);
  tr.setEncoding('hex');
  var out = [];
  var expect =
    [ '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161' ];

  tr.on('readable', function flow() {
    var chunk;
    while (null !== (chunk = tr.read(10)))
      out.push(chunk);
  });

  tr.on('end', function() {
    t.same(out, expect);
    t.end();
  });
});

test('setEncoding hex with read(13)', function(t) {
  var tr = new TestReader(100);
  tr.setEncoding('hex');
  var out = [];
  var expect =
    [ "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "16161" ];

  tr.on('readable', function flow() {
    console.log("readable once")
    var chunk;
    while (null !== (chunk = tr.read(13)))
      out.push(chunk);
  });

  tr.on('end', function() {
    console.log("END")
    t.same(out, expect);
    t.end();
  });
});

test('setEncoding base64', function(t) {
  var tr = new TestReader(100);
  tr.setEncoding('base64');
  var out = [];
  var expect =
    [ 'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYQ==' ];

  tr.on('readable', function flow() {
    var chunk;
    while (null !== (chunk = tr.read(10)))
      out.push(chunk);
  });

  tr.on('end', function() {
    t.same(out, expect);
    t.end();
  });
});

test('encoding: utf8', function(t) {
  var tr = new TestReader(100, { encoding: 'utf8' });
  var out = [];
  var expect =
    [ 'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa',
      'aaaaaaaaaa' ];

  tr.on('readable', function flow() {
    var chunk;
    while (null !== (chunk = tr.read(10)))
      out.push(chunk);
  });

  tr.on('end', function() {
    t.same(out, expect);
    t.end();
  });
});


test('encoding: hex', function(t) {
  var tr = new TestReader(100, { encoding: 'hex' });
  var out = [];
  var expect =
    [ '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161',
      '6161616161' ];

  tr.on('readable', function flow() {
    var chunk;
    while (null !== (chunk = tr.read(10)))
      out.push(chunk);
  });

  tr.on('end', function() {
    t.same(out, expect);
    t.end();
  });
});

test('encoding: hex with read(13)', function(t) {
  var tr = new TestReader(100, { encoding: 'hex' });
  var out = [];
  var expect =
    [ "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "1616161616161",
      "6161616161616",
      "16161" ];

  tr.on('readable', function flow() {
    var chunk;
    while (null !== (chunk = tr.read(13)))
      out.push(chunk);
  });

  tr.on('end', function() {
    t.same(out, expect);
    t.end();
  });
});

test('encoding: base64', function(t) {
  var tr = new TestReader(100, { encoding: 'base64' });
  var out = [];
  var expect =
    [ 'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYWFhYWFh',
      'YWFhYWFhYW',
      'FhYQ==' ];

  tr.on('readable', function flow() {
    var chunk;
    while (null !== (chunk = tr.read(10)))
      out.push(chunk);
  });

  tr.on('end', function() {
    t.same(out, expect);
    t.end();
  });
});
