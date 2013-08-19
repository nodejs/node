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

var common = require('../common');
var assert = require('assert');
var Stream = require('stream').Stream;

(function testErrorListenerCatches() {
  var source = new Stream();
  var dest = new Stream();

  source.pipe(dest);

  var gotErr = null;
  source.on('error', function(err) {
    gotErr = err;
  });

  var err = new Error('This stream turned into bacon.');
  source.emit('error', err);
  assert.strictEqual(gotErr, err);
})();

(function testErrorWithoutListenerThrows() {
  var source = new Stream();
  var dest = new Stream();

  source.pipe(dest);

  var err = new Error('This stream turned into bacon.');

  var gotErr = null;
  try {
    source.emit('error', err);
  } catch (e) {
    gotErr = e;
  }

  assert.strictEqual(gotErr, err);
})();

(function testErrorWithRemovedListenerThrows() {
  var EE = require('events').EventEmitter;
  var R = Stream.Readable;
  var W = Stream.Writable;

  var r = new R;
  var w = new W;
  var removed = false;
  var didTest = false;

  process.on('exit', function() {
    assert(didTest);
    console.log('ok');
  });

  r._read = function() {
    setTimeout(function() {
      assert(removed);
      assert.throws(function() {
        w.emit('error', new Error('fail'));
      });
      didTest = true;
    });
  };

  w.on('error', myOnError);
  r.pipe(w);
  w.removeListener('error', myOnError);
  removed = true;

  function myOnError(er) {
    throw new Error('this should not happen');
  }
})();

(function testErrorWithRemovedListenerThrows() {
  var EE = require('events').EventEmitter;
  var R = Stream.Readable;
  var W = Stream.Writable;

  var r = new R;
  var w = new W;
  var removed = false;
  var didTest = false;
  var caught = false;

  process.on('exit', function() {
    assert(didTest);
    console.log('ok');
  });

  r._read = function() {
    setTimeout(function() {
      assert(removed);
      w.emit('error', new Error('fail'));
      didTest = true;
    });
  };

  w.on('error', myOnError);
  w._write = function() {};

  r.pipe(w);
  // Removing some OTHER random listener should not do anything
  w.removeListener('error', function() {});
  removed = true;

  function myOnError(er) {
    assert(!caught);
    caught = true;
  }
})();
