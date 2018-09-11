'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
  self.importScripts('../resources/constructor-ordering.js');
}

const operations = [
  op('get', 'size'),
  op('get', 'highWaterMark'),
  op('get', 'type'),
  op('validate', 'type'),
  op('validate', 'size'),
  op('tonumber', 'highWaterMark'),
  op('validate', 'highWaterMark'),
  op('get', 'pull'),
  op('validate', 'pull'),
  op('get', 'cancel'),
  op('validate', 'cancel'),
  op('get', 'autoAllocateChunkSize'),
  op('tonumber', 'autoAllocateChunkSize'),
  op('validate', 'autoAllocateChunkSize'),
  op('get', 'start'),
  op('validate', 'start')
];

for (const failureOp of operations) {
  test(() => {
    const record = new OpRecorder(failureOp);
    const underlyingSource = createRecordingObjectWithProperties(record, ['start', 'pull', 'cancel']);

    // The valid value for "type" is "bytes", so set it separately.
    defineCheckedProperty(record, underlyingSource, 'type', () => record.check('type') ? 'invalid' : 'bytes');

    // autoAllocateChunkSize is a special case because it has a tonumber step.
    defineCheckedProperty(record, underlyingSource, 'autoAllocateChunkSize',
                          () => createRecordingNumberObject(record, 'autoAllocateChunkSize'));

    const strategy = createRecordingStrategy(record);

    try {
      new ReadableStream(underlyingSource, strategy);
      assert_unreached('constructor should throw');
    } catch (e) {
      assert_equals(typeof e, 'object', 'e should be an object');
    }

    assert_equals(record.actual(), expectedAsString(operations, failureOp),
                  'operations should be performed in the right order');
  }, `ReadableStream constructor should stop after ${failureOp} fails`);
}

done();
