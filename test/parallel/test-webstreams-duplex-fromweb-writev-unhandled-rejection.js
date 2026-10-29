'use strict';

// Regression test for https://github.com/nodejs/node/issues/62199
//
// When Duplex.fromWeb is corked, writes are batched into _writev. If destroy()
// is called in the same microtask (after uncork()), writer.ready rejects with a
// non-array value. The done() callback inside _writev unconditionally called
// error.filter(), which throws TypeError on non-arrays. This TypeError became
// an unhandled rejection that crashed the process.
//
// The same bug exists in newStreamWritableFromWritableStream (Writable.fromWeb).

const common = require('../common');
const { Duplex, Writable } = require('stream');
const { TransformStream, WritableStream } = require('stream/web');

// Exact reproduction from the issue report (davidje13).
// Before the fix: process crashes with unhandled TypeError.
// After the fix: stream closes cleanly with no unhandled rejection.
{
  const output = Duplex.fromWeb(new TransformStream());

  output.on('close', common.mustCall());

  output.cork();
  output.write('test');
  output.write('test');
  output.uncork();
  output.destroy();
}

// Same bug in Writable.fromWeb (newStreamWritableFromWritableStream).
{
  const writable = Writable.fromWeb(new WritableStream());

  writable.on('close', common.mustCall());

  writable.cork();
  writable.write('test');
  writable.write('test');
  writable.uncork();
  writable.destroy();
}

// Regression: normal cork/uncork/_writev success path must still work.
// Verifies that () => done() correctly signals success via callback().
{
  const writable = Writable.fromWeb(new WritableStream({ write() {} }));

  writable.cork();
  writable.write('foo');
  writable.write('bar');
  writable.uncork();
  writable.end(common.mustCall());
}
