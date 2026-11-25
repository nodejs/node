'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.write method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { finished } = require('stream/promises');
const { buffer } = require('stream/consumers');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function validateWrite() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write.txt');
  const fileHandle = await open(filePathForHandle, 'w');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  const stream = fileHandle.createWriteStream();
  stream.end(buffer);
  await finished(stream);

  const readFileData = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(buffer, readFileData);
}

async function validateRead() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-read.txt');
  const buf = Buffer.from('Hello world'.repeat(100), 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);
  assert.deepStrictEqual(
    await buffer(fileHandle.createReadStream()),
    buf
  );
}

// Regression test for https://github.com/nodejs/node/issues/64214: every
// createReadStream({ autoClose: false }) call used to leave behind a 'close'
// listener on the FileHandle (and an un-released internal ref), because
// autoClose: false disables autoDestroy, so the stream never goes through
// _destroy() when it finishes on its own. Repeating this past 10 iterations
// used to trigger a MaxListenersExceededWarning.
async function validateReadStreamAutoCloseFalseReleasesListener() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-read-autoclose-false.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);
  try {
    for (let i = 0; i < buf.length; i++) {
      const chunk = await buffer(fileHandle.createReadStream({
        start: i,
        end: i,
        autoClose: false,
      }));
      assert.strictEqual(chunk[0], buf[i]);
      assert.strictEqual(fileHandle.listenerCount('close'), 0);
    }
  } finally {
    await fileHandle.close();
  }
}

// Same leak, but for createWriteStream({ autoClose: false }).
async function validateWriteStreamAutoCloseFalseReleasesListener() {
  const filePathForHandle =
    path.resolve(tmpDir, 'tmp-write-autoclose-false.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  const fileHandle = await open(filePathForHandle, 'w');
  try {
    for (let i = 0; i < buf.length; i++) {
      const stream = fileHandle.createWriteStream({
        start: i,
        autoClose: false,
      });
      stream.end(buf.subarray(i, i + 1));
      await finished(stream);
      assert.strictEqual(fileHandle.listenerCount('close'), 0);
    }
  } finally {
    await fileHandle.close();
  }

  assert.deepStrictEqual(fs.readFileSync(filePathForHandle), buf);
}

// Regression test for the fix that was reverted in
// https://github.com/nodejs/node/pull/65387: a previous attempt released the
// FileHandle ref both when the stream finished on its own *and* again when
// the stream was explicitly closed/destroyed afterwards, unreffing the
// handle twice for a single stream. Explicitly closing a stream after it has
// already finished on its own (autoClose: false) is a normal thing to do and
// must not double-release the handle's reference count.
async function validateAutoCloseFalseExplicitCloseDoesNotDoubleRelease() {
  const filePathForHandle =
    path.resolve(tmpDir, 'tmp-read-autoclose-false-explicit-close.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);
  // Register this before anything closes the handle: FileHandleOperations
  // .close() unconditionally closes the handle once the stream is
  // destroyed (that's how `autoClose: true` implicitly closes the handle),
  // so the explicit stream.close() below is expected to trigger it. This
  // listener itself accounts for one 'close' listener throughout, on top of
  // whatever the stream adds/removes.
  const closed = new Promise((resolve) => {
    fileHandle.once('close', common.mustCall(resolve));
  });

  const stream = fileHandle.createReadStream({
    start: 0,
    end: 0,
    autoClose: false,
  });
  await buffer(stream);
  // Only the listener registered above remains; the stream's own listener
  // was released when it finished on its own.
  assert.strictEqual(fileHandle.listenerCount('close'), 1);

  // The stream already finished on its own (which already released its
  // handle ref); closing it again must be a safe no-op with respect to that
  // ref, and must not corrupt the handle's internal reference count.
  await new Promise((resolve, reject) => {
    stream.close((err) => (err ? reject(err) : resolve()));
  });
  await closed;
  assert.strictEqual(fileHandle.listenerCount('close'), 0);

  // The handle is already fully closed at this point; closing it again must
  // remain a safe, immediately-resolving no-op (it would hang or throw if
  // the ref count had gone negative).
  await fileHandle.close();
}

// The default (autoClose: true) behavior must be unaffected: finishing the
// stream still implicitly closes the FileHandle exactly once, and leaves no
// listener behind.
async function validateAutoCloseTrueStillClosesFileHandle() {
  const filePathForHandle =
    path.resolve(tmpDir, 'tmp-read-autoclose-true.txt');
  const buf = Buffer.from('Hello world', 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);
  const closed = new Promise((resolve) => {
    fileHandle.once('close', common.mustCall(resolve));
  });

  assert.deepStrictEqual(await buffer(fileHandle.createReadStream()), buf);
  await closed;
  assert.strictEqual(fileHandle.listenerCount('close'), 0);
}

Promise.all([
  validateWrite(),
  validateRead(),
  validateReadStreamAutoCloseFalseReleasesListener(),
  validateWriteStreamAutoCloseFalseReleasesListener(),
  validateAutoCloseFalseExplicitCloseDoesNotDoubleRelease(),
  validateAutoCloseTrueStillClosesFileHandle(),
]).then(common.mustCall());
