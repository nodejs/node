// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  from,
  fromSync,
  text,
  textSync,
} = require('stream/iter');

// =============================================================================
// textSync / text
// =============================================================================

async function testTextSyncBasic() {
  const data = textSync(fromSync('hello text'));
  assert.strictEqual(data, 'hello text');
}

async function testTextAsync() {
  const data = await text(from('hello async text'));
  assert.strictEqual(data, 'hello async text');
}

async function testTextEncoding() {
  // Default encoding is utf-8
  const data = await text(from('café'));
  assert.strictEqual(data, 'café');
}

// =============================================================================
// Text encoding tests
// =============================================================================

async function testTextNonUtf8Encoding() {
  // Latin-1 encoding
  const latin1Bytes = new Uint8Array([0xE9, 0xE8, 0xEA]); // é, è, ê in latin1
  const result = await text(from(latin1Bytes), { encoding: 'iso-8859-1' });
  assert.strictEqual(result, 'éèê');
}

async function testTextSyncNonUtf8Encoding() {
  const latin1Bytes = new Uint8Array([0xE9, 0xE8, 0xEA]);
  const result = textSync(fromSync(latin1Bytes), { encoding: 'iso-8859-1' });
  assert.strictEqual(result, 'éèê');
}

async function testTextInvalidUtf8() {
  // Invalid UTF-8 sequence with fatal: true should throw
  const invalid = new Uint8Array([0xFF, 0xFE]);
  await assert.rejects(
    () => text(from(invalid)),
    { name: 'TypeError' }, // TextDecoder fatal throws TypeError
  );
}

async function testTextWithLimit() {
  // Limit caps total bytes; exceeding throws ERR_OUT_OF_RANGE
  await assert.rejects(
    () => text(from('hello world'), { limit: 5 }),
    { code: 'ERR_OUT_OF_RANGE' },
  );
  // Within limit should succeed
  const result = await text(from('hello'), { limit: 10 });
  assert.strictEqual(result, 'hello');

  // Exact boundary: 'hello' is 5 UTF-8 bytes, limit: 5 should succeed
  // (source uses > not >=)
  const exact = await text(from('hello'), { limit: 5 });
  assert.strictEqual(exact, 'hello');
}

async function testTextSyncWithLimit() {
  // Sync version of limit testing
  assert.throws(
    () => textSync(fromSync('hello world'), { limit: 5 }),
    { code: 'ERR_OUT_OF_RANGE' },
  );
  const result = textSync(fromSync('hello'), { limit: 10 });
  assert.strictEqual(result, 'hello');

  // Exact boundary
  const exact = textSync(fromSync('hello'), { limit: 5 });
  assert.strictEqual(exact, 'hello');
}

async function testTextEmpty() {
  const result = await text(from(''));
  assert.strictEqual(result, '');

  const syncResult = textSync(fromSync(''));
  assert.strictEqual(syncResult, '');
}

// text() with abort signal
async function testTextWithSignal() {
  const ac = new AbortController();
  ac.abort();
  await assert.rejects(
    () => text(from('data'), { signal: ac.signal }),
    { name: 'AbortError' },
  );
}

// Multi-chunk source with a multi-byte UTF-8 character split across chunks
async function testTextMultiChunkSplitCodepoint() {
  // '€' is U+20AC, encoded as 3 UTF-8 bytes: 0xE2, 0x82, 0xAC
  // Split these bytes across two chunks to test proper re-assembly
  async function* splitSource() {
    yield [new Uint8Array([0xE2, 0x82])]; // First 2 bytes of '€'
    yield [new Uint8Array([0xAC])]; // Last byte of '€'
  }
  const result = await text(splitSource());
  assert.strictEqual(result, '€');
}

// BOM should be stripped (ignoreBOM defaults to false per spec)
async function testTextBOMStripped() {
  // UTF-8 BOM: 0xEF, 0xBB, 0xBF followed by 'hi'
  const withBOM = new Uint8Array([0xEF, 0xBB, 0xBF, 0x68, 0x69]);
  const result = await text(from(withBOM));
  assert.strictEqual(result, 'hi');
}

async function testTextSyncBOMStripped() {
  const withBOM = new Uint8Array([0xEF, 0xBB, 0xBF, 0x68, 0x69]);
  const result = textSync(fromSync(withBOM));
  assert.strictEqual(result, 'hi');
}

// Unsupported encoding throws RangeError
async function testTextUnsupportedEncodingThrowsRangeError() {
  await assert.rejects(
    () => text(from('hello'), { encoding: 'not-a-real-encoding' }),
    { name: 'RangeError' },
  );
}

function testTextSyncUnsupportedEncodingThrowsRangeError() {
  assert.throws(
    () => textSync(fromSync('hello'), { encoding: 'not-a-real-encoding' }),
    { name: 'RangeError' },
  );
}

Promise.all([
  testTextSyncBasic(),
  testTextAsync(),
  testTextEncoding(),
  testTextNonUtf8Encoding(),
  testTextSyncNonUtf8Encoding(),
  testTextInvalidUtf8(),
  testTextWithLimit(),
  testTextSyncWithLimit(),
  testTextEmpty(),
  testTextWithSignal(),
  testTextMultiChunkSplitCodepoint(),
  testTextBOMStripped(),
  testTextSyncBOMStripped(),
  testTextUnsupportedEncodingThrowsRangeError(),
  testTextSyncUnsupportedEncodingThrowsRangeError(),
]).then(common.mustCall());
