'use strict';

(() => {
  // Create a ReadableStream that will pass the tests in
  // testTransferredReadableStream(), below.
  function createOriginalReadableStream() {
    return new ReadableStream({
      start(controller) {
        controller.enqueue('a');
        controller.close();
      }
    });
  }

  // Common tests to roughly determine that |rs| is a correctly transferred
  // version of a stream created by createOriginalReadableStream().
  function testTransferredReadableStream(rs) {
    assert_equals(rs.constructor, ReadableStream,
                  'rs should be a ReadableStream in this realm');
    assert_true(rs instanceof ReadableStream,
                'instanceof check should pass');

    // Perform a brand-check on |rs| in the process of calling getReader().
    const reader = ReadableStream.prototype.getReader.call(rs);

    return reader.read().then(({value, done}) => {
      assert_false(done, 'done should be false');
      assert_equals(value, 'a', 'value should be "a"');
      return reader.read();
    }).then(({done}) => {
      assert_true(done, 'done should be true');
    });
  }

  function testMessage(msg) {
    assert_array_equals(msg.ports, [], 'there should be no ports in the event');
    return testTransferredReadableStream(msg.data);
  }

  function testMessageEvent(target) {
    return new Promise((resolve, reject) => {
      target.addEventListener('message', ev => {
        try {
          resolve(testMessage(ev));
        } catch (e) {
          reject(e);
        }
      }, {once: true});
    });
  }

  function testMessageEventOrErrorMessage(target) {
    return new Promise((resolve, reject) => {
      target.addEventListener('message', ev => {
        if (typeof ev.data === 'string') {
          // Assume it's an error message and reject with it.
          reject(ev.data);
          return;
        }

        try {
          resolve(testMessage(ev));
        } catch (e) {
          reject(e);
        }
      }, {once: true});
    });
  }

  function checkTestResults(target) {
    return new Promise((resolve, reject) => {
      target.onmessage = msg => {
        // testharness.js sends us objects which we need to ignore.
        if (typeof msg.data !== 'string')
        return;

        if (msg.data === 'OK') {
          resolve();
        } else {
          reject(msg.data);
        }
      };
    });
  }

  // These tests assume that a transferred ReadableStream will behave the same
  // regardless of how it was transferred. This enables us to simply transfer the
  // stream to ourselves.
  function createTransferredReadableStream(underlyingSource) {
    const original = new ReadableStream(underlyingSource);
    const promise = new Promise((resolve, reject) => {
      addEventListener('message', msg => {
        const rs = msg.data;
        if (rs instanceof ReadableStream) {
          resolve(rs);
        } else {
          reject(new Error(`what is this thing: "${rs}"?`));
        }
      }, {once: true});
    });
    postMessage(original, '*', [original]);
    return promise;
  }

  function recordingTransferredReadableStream(underlyingSource, strategy) {
    const original = recordingReadableStream(underlyingSource, strategy);
    const promise = new Promise((resolve, reject) => {
      addEventListener('message', msg => {
        const rs = msg.data;
        if (rs instanceof ReadableStream) {
          rs.events = original.events;
          rs.eventsWithoutPulls = original.eventsWithoutPulls;
          rs.controller = original.controller;
          resolve(rs);
        } else {
          reject(new Error(`what is this thing: "${rs}"?`));
        }
      }, {once: true});
    });
    postMessage(original, '*', [original]);
    return promise;
  }

  self.createOriginalReadableStream = createOriginalReadableStream;
  self.testMessage = testMessage;
  self.testMessageEvent = testMessageEvent;
  self.testMessageEventOrErrorMessage = testMessageEventOrErrorMessage;
  self.checkTestResults = checkTestResults;
  self.createTransferredReadableStream = createTransferredReadableStream;
  self.recordingTransferredReadableStream = recordingTransferredReadableStream;

})();
