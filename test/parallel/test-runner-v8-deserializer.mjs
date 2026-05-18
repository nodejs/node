// Flags: --expose-internals --no-warnings

import * as common from '../common/index.mjs';
import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert';
import { finished } from 'node:stream/promises';
import { DefaultSerializer } from 'node:v8';
import serializer from 'internal/test_runner/reporter/v8-serializer';
import runner from 'internal/test_runner/runner';

async function toArray(chunks) {
  const arr = [];
  for await (const i of chunks) arr.push(i);
  return arr;
}

const diagnosticEvent = {
  type: 'test:diagnostic',
  data: { nesting: 0, details: {}, message: 'diagnostic' },
};
const chunks = await toArray(serializer([diagnosticEvent]));
const defaultSerializer = new DefaultSerializer();
defaultSerializer.writeHeader();
const headerLength = defaultSerializer.releaseBuffer().length;
const headerOnly = Buffer.from([0xff, 0x0f]);
const oversizedLengthHeader = Buffer.from([0xff, 0x0f, 0x7f, 0xff, 0xff, 0xff]);
const truncatedLengthHeader = Buffer.from([0xff, 0x0f, 0x00, 0x01, 0x00, 0x00]);
// Expected stdout for oversizedLengthHeader: first byte is emitted via
// String.fromCharCode (byte-by-byte fallback in #drainRawBuffer), remaining
// bytes go through the nonSerialized UTF-8 decode path in #processRawBuffer.
const oversizedLengthStdout = String.fromCharCode(oversizedLengthHeader[0]) +
  Buffer.from(oversizedLengthHeader.subarray(1)).toString('utf-8');

function collectStdout(reported) {
  return reported
    .filter((event) => event.type === 'test:stdout')
    .map((event) => event.data.message)
    .join('');
}

describe('v8 deserializer', common.mustCall(() => {
  let fileTest;
  let reported;
  beforeEach(common.mustCallAtLeast(() => {
    reported = [];
    fileTest = new runner.FileTest({
      name: 'filetest',
      harness: { config: {} },
    });
    fileTest.reporter.on('data', (data) => reported.push(data));
    assert(fileTest.isClearToSend());
  }));

  async function collectReported(chunks) {
    chunks.forEach((chunk) => fileTest.parseMessage(chunk));
    fileTest.drain();
    fileTest.reporter.end();
    await finished(fileTest.reporter);
    return reported;
  }

  it('should do nothing when no chunks', async () => {
    const reported = await collectReported([]);
    assert.deepStrictEqual(reported, []);
  });

  it('should deserialize a chunk with no serialization', async () => {
    const reported = await collectReported([Buffer.from('unknown')]);
    assert.deepStrictEqual(reported, [
      { data: { __proto__: null, file: 'filetest', message: 'unknown' }, type: 'test:stdout' },
    ]);
  });

  it('should deserialize a serialized chunk', async () => {
    const reported = await collectReported(chunks);
    assert.deepStrictEqual(reported, [diagnosticEvent]);
  });

  it('should deserialize a serialized chunk after non-serialized chunk', async () => {
    const reported = await collectReported([Buffer.concat([Buffer.from('unknown'), ...chunks])]);
    assert.deepStrictEqual(reported, [
      { data: { __proto__: null, file: 'filetest', message: 'unknown' }, type: 'test:stdout' },
      diagnosticEvent,
    ]);
  });

  it('should deserialize a serialized chunk before non-serialized output', async () => {
    const reported = await collectReported([Buffer.concat([ ...chunks, Buffer.from('unknown')])]);
    assert.deepStrictEqual(reported, [
      diagnosticEvent,
      { data: { __proto__: null, file: 'filetest', message: 'unknown' }, type: 'test:stdout' },
    ]);
  });

  it('should not hang when buffer starts with v8Header followed by oversized length', async () => {
    // Regression test for https://github.com/nodejs/node/issues/62693
    // FF 0F is the v8 serializer header; the next 4 bytes are read as a
    // big-endian message size.  0x7FFFFFFF far exceeds any actual buffer
    // size, causing #processRawBuffer to make no progress and
    // #drainRawBuffer to loop forever without the no-progress guard.
    const reported = await collectReported([oversizedLengthHeader]);
    assert.partialDeepStrictEqual(
      reported,
      Array.from({ length: reported.length }, () => ({ type: 'test:stdout' })),
    );
    assert.strictEqual(collectStdout(reported), oversizedLengthStdout);
  });

  it('should flush incomplete v8 frame as stdout and keep prior valid data', async () => {
    // A valid non-serialized message followed by bytes that look like
    // a v8 header with a truncated/oversized length.
    const reported = await collectReported([
      Buffer.from('hello'),
      truncatedLengthHeader,
    ]);
    assert.strictEqual(collectStdout(reported), `hello${truncatedLengthHeader.toString('latin1')}`);
  });

  it('should flush v8Header-only bytes as stdout when stream ends', async () => {
    // Just the two-byte v8 header with no size field at all.
    const reported = await collectReported([headerOnly]);
    assert(reported.every((event) => event.type === 'test:stdout'));
    assert.strictEqual(collectStdout(reported), headerOnly.toString('latin1'));
  });

  it('should resync and parse valid messages after false v8 header', async () => {
    // A false v8 header (FF 0F + oversized length) followed by a
    // legitimate serialized message. The parser must skip the corrupt
    // bytes and still deserialize the real message.
    const reported = await collectReported([
      oversizedLengthHeader,
      ...chunks,
    ]);
    assert.deepStrictEqual(reported.at(-1), diagnosticEvent);
    assert.strictEqual(reported.filter((event) => event.type === 'test:diagnostic').length, 1);
    assert.strictEqual(collectStdout(reported), oversizedLengthStdout);
  });

  it('should preserve a false v8 header split across chunks', async () => {
    const reported = await collectReported([
      oversizedLengthHeader.subarray(0, 1),
      oversizedLengthHeader.subarray(1),
    ]);
    assert(reported.every((event) => event.type === 'test:stdout'));
    assert.strictEqual(collectStdout(reported), oversizedLengthStdout);
  });

  const headerPosition = headerLength * 2 + 4;
  for (let i = 0; i < headerPosition + 5; i++) {
    const message = `should deserialize a serialized message split into two chunks {...${i},${i + 1}...}`;
    it(message, async () => {
      const data = chunks[0];
      const reported = await collectReported([data.subarray(0, i), data.subarray(i)]);
      assert.deepStrictEqual(reported, [
        diagnosticEvent,
      ]);
    });

    it(`${message} wrapped by non-serialized data`, async () => {
      const data = chunks[0];
      const reported = await collectReported([
        Buffer.concat([Buffer.from('unknown'), data.subarray(0, i)]),
        Buffer.concat([data.subarray(i), Buffer.from('unknown')]),
      ]);
      assert.deepStrictEqual(reported, [
        { data: { __proto__: null, file: 'filetest', message: 'unknown' }, type: 'test:stdout' },
        diagnosticEvent,
        { data: { __proto__: null, file: 'filetest', message: 'unknown' }, type: 'test:stdout' },
      ]);
    }
    );
  }

}));
