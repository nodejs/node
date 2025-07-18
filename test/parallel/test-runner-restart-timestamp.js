'use strict';
const common = require('../common');
const assert = require('assert');
const { TestsStream } = require('../../lib/internal/test_runner/tests_stream');

{
  const stream = new TestsStream();

  let called = false;

  // Wrap the timeStamp method to check it was called
  stream.timeStamp = common.mustCall((type, data) => {
    assert.strictEqual(type, 'test:restarted');
    assert.ok(data.file);
    called = true;
  });

  // Simulate emitting the event
  stream.emit('test:restarted', { file: 'timestamp.test.js' });

  // Optionally check manually if needed
  assert.ok(called, 'timeStamp should be called on test:restarted event');
}
