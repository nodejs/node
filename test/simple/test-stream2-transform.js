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
var PassThrough = require('_stream_passthrough');
var Transform = require('_stream_transform');

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

/////

test('passthrough', function(t) {
  var pt = new PassThrough();

  pt.write(new Buffer('foog'));
  pt.write(new Buffer('bark'));
  pt.write(new Buffer('bazy'));
  pt.write(new Buffer('kuel'));
  pt.end();

  t.equal(pt.read(5).toString(), 'foogb');
  t.equal(pt.read(5).toString(), 'arkba');
  t.equal(pt.read(5).toString(), 'zykue');
  t.equal(pt.read(5).toString(), 'l');
  t.end();
});

test('simple transform', function(t) {
  var pt = new Transform;
  pt._transform = function(c, output, cb) {
    var ret = new Buffer(c.length);
    ret.fill('x');
    output(ret);
    cb();
  };

  pt.write(new Buffer('foog'));
  pt.write(new Buffer('bark'));
  pt.write(new Buffer('bazy'));
  pt.write(new Buffer('kuel'));
  pt.end();

  t.equal(pt.read(5).toString(), 'xxxxx');
  t.equal(pt.read(5).toString(), 'xxxxx');
  t.equal(pt.read(5).toString(), 'xxxxx');
  t.equal(pt.read(5).toString(), 'x');
  t.end();
});

test('async passthrough', function(t) {
  var pt = new Transform;
  pt._transform = function(chunk, output, cb) {
    setTimeout(function() {
      output(chunk);
      cb();
    }, 10);
  };

  pt.write(new Buffer('foog'));
  pt.write(new Buffer('bark'));
  pt.write(new Buffer('bazy'));
  pt.write(new Buffer('kuel'));
  pt.end();

  setTimeout(function() {
    t.equal(pt.read(5).toString(), 'foogb');
    t.equal(pt.read(5).toString(), 'arkba');
    t.equal(pt.read(5).toString(), 'zykue');
    t.equal(pt.read(5).toString(), 'l');
    t.end();
  }, 100);
});

test('assymetric transform (expand)', function(t) {
  var pt = new Transform;

  // emit each chunk 2 times.
  pt._transform = function(chunk, output, cb) {
    setTimeout(function() {
      output(chunk);
      setTimeout(function() {
        output(chunk);
        cb();
      }, 10)
    }, 10);
  };

  pt.write(new Buffer('foog'));
  pt.write(new Buffer('bark'));
  pt.write(new Buffer('bazy'));
  pt.write(new Buffer('kuel'));
  pt.end();

  setTimeout(function() {
    t.equal(pt.read(5).toString(), 'foogf');
    t.equal(pt.read(5).toString(), 'oogba');
    t.equal(pt.read(5).toString(), 'rkbar');
    t.equal(pt.read(5).toString(), 'kbazy');
    t.equal(pt.read(5).toString(), 'bazyk');
    t.equal(pt.read(5).toString(), 'uelku');
    t.equal(pt.read(5).toString(), 'el');
    t.end();
  }, 200);
});

test('assymetric transform (compress)', function(t) {
  var pt = new Transform;

  // each output is the first char of 3 consecutive chunks,
  // or whatever's left.
  pt.state = '';

  pt._transform = function(chunk, output, cb) {
    if (!chunk)
      chunk = '';
    var s = chunk.toString();
    setTimeout(function() {
      this.state += s.charAt(0);
      if (this.state.length === 3) {
        output(new Buffer(this.state));
        this.state = '';
      }
      cb();
    }.bind(this), 10);
  };

  pt._flush = function(output, cb) {
    // just output whatever we have.
    setTimeout(function() {
      output(new Buffer(this.state));
      this.state = '';
      cb();
    }.bind(this), 10);
  };

  pt._writableState.lowWaterMark = 3;

  pt.write(new Buffer('aaaa'));
  pt.write(new Buffer('bbbb'));
  pt.write(new Buffer('cccc'));
  pt.write(new Buffer('dddd'));
  pt.write(new Buffer('eeee'));
  pt.write(new Buffer('aaaa'));
  pt.write(new Buffer('bbbb'));
  pt.write(new Buffer('cccc'));
  pt.write(new Buffer('dddd'));
  pt.write(new Buffer('eeee'));
  pt.write(new Buffer('aaaa'));
  pt.write(new Buffer('bbbb'));
  pt.write(new Buffer('cccc'));
  pt.write(new Buffer('dddd'));
  pt.end();

  // 'abcdeabcdeabcd'
  setTimeout(function() {
    t.equal(pt.read(5).toString(), 'abcde');
    t.equal(pt.read(5).toString(), 'abcde');
    t.equal(pt.read(5).toString(), 'abcd');
    t.end();
  }, 200);
});


test('passthrough event emission', function(t) {
  var pt = new PassThrough({
    lowWaterMark: 0
  });
  var emits = 0;
  pt.on('readable', function() {
    var state = pt._readableState;
    console.error('>>> emit readable %d', emits);
    emits++;
  });

  var i = 0;

  pt.write(new Buffer('foog'));

  console.error('need emit 0');
  pt.write(new Buffer('bark'));

  console.error('should have emitted readable now 1 === %d', emits);
  t.equal(emits, 1);

  t.equal(pt.read(5).toString(), 'foogb');
  t.equal(pt.read(5) + '', 'null');

  console.error('need emit 1');

  pt.write(new Buffer('bazy'));
  console.error('should have emitted, but not again');
  pt.write(new Buffer('kuel'));

  console.error('should have emitted readable now 2 === %d', emits);
  t.equal(emits, 2);

  t.equal(pt.read(5).toString(), 'arkba');
  t.equal(pt.read(5).toString(), 'zykue');
  t.equal(pt.read(5), null);

  console.error('need emit 2');

  pt.end();

  t.equal(emits, 3);

  t.equal(pt.read(5).toString(), 'l');
  t.equal(pt.read(5), null);

  console.error('should not have emitted again');
  t.equal(emits, 3);
  t.end();
});

test('passthrough event emission reordered', function(t) {
  var pt = new PassThrough;
  var emits = 0;
  pt.on('readable', function() {
    console.error('emit readable', emits)
    emits++;
  });

  pt.write(new Buffer('foog'));
  console.error('need emit 0');
  pt.write(new Buffer('bark'));
  console.error('should have emitted readable now 1 === %d', emits);
  t.equal(emits, 1);

  t.equal(pt.read(5).toString(), 'foogb');
  t.equal(pt.read(5), null);

  console.error('need emit 1');
  pt.once('readable', function() {
    t.equal(pt.read(5).toString(), 'arkba');

    t.equal(pt.read(5), null);

    console.error('need emit 2');
    pt.once('readable', function() {
      t.equal(pt.read(5).toString(), 'zykue');
      t.equal(pt.read(5), null);
      pt.once('readable', function() {
        t.equal(pt.read(5).toString(), 'l');
        t.equal(pt.read(5), null);
        t.equal(emits, 4);
        t.end();
      });
      pt.end();
    });
    pt.write(new Buffer('kuel'));
  });

  pt.write(new Buffer('bazy'));
});

test('passthrough facaded', function(t) {
  console.error('passthrough facaded');
  var pt = new PassThrough;
  var datas = [];
  pt.on('data', function(chunk) {
    datas.push(chunk.toString());
  });

  pt.on('end', function() {
    t.same(datas, ['foog', 'bark', 'bazy', 'kuel']);
    t.end();
  });

  pt.write(new Buffer('foog'));
  setTimeout(function() {
    pt.write(new Buffer('bark'));
    setTimeout(function() {
      pt.write(new Buffer('bazy'));
      setTimeout(function() {
        pt.write(new Buffer('kuel'));
        setTimeout(function() {
          pt.end();
        }, 10);
      }, 10);
    }, 10);
  }, 10);
});
