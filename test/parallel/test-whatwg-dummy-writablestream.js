'use strict';

const test = require('node:test');
const assert = require('node:assert');
const { DummyWritableStream } = require('stream/web');

test('writes to dummy stream', async () => {
    const stream = new DummyWritableStream();
    const writer = stream.getWriter();
    await writer.write('hello'); // ✅ This is valid
    await writer.close();
});

test('DummyWritableStream allows writing and closing without error', async () => {
    const stream = new DummyWritableStream();
    const writer = stream.getWriter();

    // Writing should not throw
    await assert.doesNotReject(() => writer.write('test'));

    // // Closing should not throw
    await assert.doesNotReject(() => writer.close());
});

test('DummyWritableStream ignores input and acts as a no-op', async () => {
    const stream = new DummyWritableStream();
    const writer = stream.getWriter();

    await writer.write('foo');
    await writer.write('bar');
    await writer.close();
    writer.releaseLock();
    
    assert.strictEqual(stream.locked, false);
});