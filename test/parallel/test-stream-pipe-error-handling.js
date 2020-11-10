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

'use strict';
const common = require('../common');
const assert = require('assert');
const Stream = require('stream').Stream;

{
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
}

{
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
}

{
  const R = Stream.Readable;
  const W = Stream.Writable;

  const r = new R({ autoDestroy: false });
  const w = new W({ autoDestroy: false });
  let removed = false;

  r._read = common.mustCall(function() {
    setTimeout(common.mustCall(function() {
      assert(removed);
      assert.throws(function() {
        w.emit('error', new Error('fail'));
      }, /^Error: fail$/);
    }), 1);
  });

  w.on('error', myOnError);
  r.pipe(w);
  w.removeListener('error', myOnError);
  removed = true;

  function myOnError() {
    throw new Error('this should not happen');
  }
}

{
  const R = Stream.Readable;
  const W = Stream.Writable;

  const r = new R();
  const w = new W();
  let removed = false;

  r._read = common.mustCall(function() {
    setTimeout(common.mustCall(function() {
      assert(removed);
      w.emit('error', new Error('fail'));
    }), 1);
  });

  w.on('error', common.mustCall());
  w._write = () => {};

  r.pipe(w);
  // Removing some OTHER random listener should not do anything
  w.removeListener('error', () => {});
  removed = true;
}
