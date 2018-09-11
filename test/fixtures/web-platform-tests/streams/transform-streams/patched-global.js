'use strict';

// Tests which patch the global environment are kept separate to avoid interfering with other tests.

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
}

// eslint-disable-next-line no-extend-native, accessor-pairs
Object.defineProperty(Object.prototype, 'highWaterMark', {
  set() { throw new Error('highWaterMark setter called'); }
});

// eslint-disable-next-line no-extend-native, accessor-pairs
Object.defineProperty(Object.prototype, 'size', {
  set() { throw new Error('size setter called'); }
});

test(() => {
  assert_not_equals(new TransformStream(), null, 'constructor should work');
}, 'TransformStream constructor should not call setters for highWaterMark or size');

test(t => {
  /* eslint-disable no-native-reassign */

  const oldReadableStream = ReadableStream;
  const oldWritableStream = WritableStream;
  const getReader = ReadableStream.prototype.getReader;
  const getWriter = WritableStream.prototype.getWriter;

  // Replace ReadableStream and WritableStream with broken versions.
  ReadableStream = function () {
    throw new Error('Called the global ReadableStream constructor');
  };
  WritableStream = function () {
    throw new Error('Called the global WritableStream constructor');
  };
  t.add_cleanup(() => {
    ReadableStream = oldReadableStream;
    WritableStream = oldWritableStream;
  });

  const ts = new TransformStream();

  // Just to be sure, ensure the readable and writable pass brand checks.
  assert_not_equals(getReader.call(ts.readable), undefined,
                    'getReader should work when called on ts.readable');
  assert_not_equals(getWriter.call(ts.writable), undefined,
                    'getWriter should work when called on ts.writable');
  /* eslint-enable no-native-reassign */
}, 'TransformStream should use the original value of ReadableStream and WritableStream');

done();
