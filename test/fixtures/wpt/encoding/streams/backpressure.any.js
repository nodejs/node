// META: global=worker

'use strict';

const classes = [
  {
    constructor: TextDecoderStream,
    input: new Uint8Array([65])
  },
  {
    constructor: TextEncoderStream,
    input: 'A'
  }
];

const microtasksRun = () => new Promise(resolve => step_timeout(resolve, 0));

for (const streamClass of classes) {
  promise_test(async () => {
    const stream = new streamClass.constructor();
    const writer = stream.writable.getWriter();
    const reader = stream.readable.getReader();
    const events = [];
    await microtasksRun();
    const writePromise = writer.write(streamClass.input);
    writePromise.then(() => events.push('write'));
    await microtasksRun();
    events.push('paused');
    await reader.read();
    events.push('read');
    await writePromise;
    assert_array_equals(events, ['paused', 'read', 'write'],
                        'write should happen after read');
  }, 'write() should not complete until read relieves backpressure for ' +
     `${streamClass.constructor.name}`);

  promise_test(async () => {
    const stream = new streamClass.constructor();
    const writer = stream.writable.getWriter();
    const reader = stream.readable.getReader();
    const events = [];
    await microtasksRun();
    const readPromise1 = reader.read();
    readPromise1.then(() => events.push('read1'));
    const writePromise1 = writer.write(streamClass.input);
    const writePromise2 = writer.write(streamClass.input);
    writePromise1.then(() => events.push('write1'));
    writePromise2.then(() => events.push('write2'));
    await microtasksRun();
    events.push('paused');
    const readPromise2 = reader.read();
    readPromise2.then(() => events.push('read2'));
    await Promise.all([writePromise1, writePromise2,
                       readPromise1, readPromise2]);
    assert_array_equals(events, ['read1', 'write1', 'paused', 'read2',
                                 'write2'],
                        'writes should not happen before read2');
  }, 'additional writes should wait for backpressure to be relieved for ' +
     `class ${streamClass.constructor.name}`);
}
