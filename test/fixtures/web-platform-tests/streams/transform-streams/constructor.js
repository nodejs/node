'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
  self.importScripts('../resources/constructor-ordering.js');
}

const operations = [
  op('get', 'size', 'writable'),
  op('get', 'highWaterMark', 'writable'),
  op('get', 'size', 'readable'),
  op('get', 'highWaterMark', 'readable'),
  op('get', 'writableType'),
  op('validate', 'writableType'),
  op('validate', 'size', 'writable'),
  op('tonumber', 'highWaterMark', 'writable'),
  op('validate', 'highWaterMark', 'writable'),
  op('get', 'readableType'),
  op('validate', 'readableType'),
  op('validate', 'size', 'readable'),
  op('tonumber', 'highWaterMark', 'readable'),
  op('validate', 'highWaterMark', 'readable'),
  op('get', 'transform'),
  op('validate', 'transform'),
  op('get', 'flush'),
  op('validate', 'flush'),
  op('get', 'start'),
  op('validate', 'start')
];

for (const failureOp of operations) {
  test(() => {
    const record = new OpRecorder(failureOp);
    const transformer = createRecordingObjectWithProperties(
        record, ['readableType', 'writableType', 'start', 'transform', 'flush']);
    const writableStrategy = createRecordingStrategy(record, 'writable');
    const readableStrategy = createRecordingStrategy(record, 'readable');

    try {
      new TransformStream(transformer, writableStrategy, readableStrategy);
      assert_unreached('constructor should throw');
    } catch (e) {
      assert_equals(typeof e, 'object', 'e should be an object');
    }

    assert_equals(record.actual(), expectedAsString(operations, failureOp),
                  'operations should be performed in the right order');
  }, `TransformStream constructor should stop after ${failureOp} fails`);
}

done();
