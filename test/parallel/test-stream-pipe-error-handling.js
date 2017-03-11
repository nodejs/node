'use strict';
const common = require('../common');
const assert = require('assert');
const Stream = require('stream').Stream;

(function testErrorListenerCatches() {
  const source = new Stream();
  const dest = new Stream();

  source.pipe(dest);

  let gotErr = null;
  source.on('error', function(err) {
    gotErr = err;
  });

  const err = new Error('This stream turned into bacon.');
  source.emit('error', err);
  assert.strictEqual(gotErr, err);
})();

(function testErrorWithoutListenerThrows() {
  const source = new Stream();
  const dest = new Stream();

  source.pipe(dest);

  const err = new Error('This stream turned into bacon.');

  let gotErr = null;
  try {
    source.emit('error', err);
  } catch (e) {
    gotErr = e;
  }

  assert.strictEqual(gotErr, err);
})();

(function testErrorWithRemovedListenerThrows() {
  const R = Stream.Readable;
  const W = Stream.Writable;

  const r = new R();
  const w = new W();
  let removed = false;
  let didTest = false;

  process.on('exit', function() {
    assert(didTest);
    console.log('ok');
  });

  r._read = common.mustCall(function() {
    setTimeout(function() {
      assert(removed);
      assert.throws(function() {
        w.emit('error', new Error('fail'));
      });
      didTest = true;
    });
  });

  w.on('error', myOnError);
  r.pipe(w);
  w.removeListener('error', myOnError);
  removed = true;

  function myOnError(er) {
    throw new Error('this should not happen');
  }
})();

(function testErrorWithRemovedListenerThrows() {
  const R = Stream.Readable;
  const W = Stream.Writable;

  const r = new R();
  const w = new W();
  let removed = false;
  let didTest = false;
  let caught = false;

  process.on('exit', function() {
    assert(didTest);
    console.log('ok');
  });

  r._read = common.mustCall(function() {
    setTimeout(function() {
      assert(removed);
      w.emit('error', new Error('fail'));
      didTest = true;
    });
  });

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
