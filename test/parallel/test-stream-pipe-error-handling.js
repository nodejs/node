'use strict';
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

  var r = new R();
  var w = new W();
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

  var r = new R();
  var w = new W();
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
