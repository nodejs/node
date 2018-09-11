'use strict';

// Tests which patch the global environment are kept separate to avoid
// interfering with other tests.

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
}

const ReadableStream_prototype_locked_get =
      Object.getOwnPropertyDescriptor(ReadableStream.prototype, 'locked').get;

// Verify that |rs| passes the brand check as a readable stream.
function isReadableStream(rs) {
  try {
    ReadableStream_prototype_locked_get.call(rs);
    return true;
  } catch (e) {
    return false;
  }
}

test(t => {
  const rs = new ReadableStream();

  const trappedProperties = ['highWaterMark', 'size', 'start', 'type', 'mode'];
  for (const property of trappedProperties) {
    // eslint-disable-next-line no-extend-native, accessor-pairs
    Object.defineProperty(Object.prototype, property, {
      get() { throw new Error(`${property} getter called`); },
      configurable: true
    });
  }
  t.add_cleanup(() => {
    for (const property of trappedProperties) {
      delete Object.prototype[property];
    }
  });

  const [branch1, branch2] = rs.tee();
  assert_true(isReadableStream(branch1), 'branch1 should be a ReadableStream');
  assert_true(isReadableStream(branch2), 'branch2 should be a ReadableStream');
}, 'ReadableStream tee() should not touch Object.prototype properties');

test(t => {
  const rs = new ReadableStream();

  const oldReadableStream = self.ReadableStream;

  /* eslint-disable no-native-reassign */
  self.ReadableStream = function() {
    throw new Error('ReadableStream called on global object');
  };

  t.add_cleanup(() => {
    self.ReadableStream = oldReadableStream;
  });

  const [branch1, branch2] = rs.tee();

  assert_true(isReadableStream(branch1), 'branch1 should be a ReadableStream');
  assert_true(isReadableStream(branch2), 'branch2 should be a ReadableStream');

  /* eslint-enable no-native-reassign */
}, 'ReadableStream tee() should not call the global ReadableStream');

done();
