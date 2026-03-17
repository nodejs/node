// Flags: --experimental-raw-imports
import '../common/index.mjs';
import assert from 'assert';
import { Buffer } from 'buffer';

import staticText from '../fixtures/file-to-read-without-bom.txt' with { type: 'text' };
import staticTextWithBOM from '../fixtures/file-to-read-with-bom.txt' with { type: 'text' };
import staticBytes from '../fixtures/file-to-read-without-bom.txt' with { type: 'bytes' };

const expectedText = 'abc\ndef\nghi\n';
const expectedBytes = Buffer.from(expectedText);

assert.strictEqual(staticText, expectedText);
assert.strictEqual(staticTextWithBOM, expectedText);

assert.ok(staticBytes instanceof Uint8Array);
assert.deepStrictEqual(Buffer.from(staticBytes), expectedBytes);

const dynamicText = await import('../fixtures/file-to-read-without-bom.txt', {
  with: { type: 'text' },
});
assert.strictEqual(dynamicText.default, expectedText);

const dynamicBytes = await import('../fixtures/file-to-read-without-bom.txt', {
  with: { type: 'bytes' },
});
assert.deepStrictEqual(Buffer.from(dynamicBytes.default), expectedBytes);

const dataText = await import('data:text/plain,hello%20world', {
  with: { type: 'text' },
});
assert.strictEqual(dataText.default, 'hello world');

const dataBytes = await import('data:text/plain,hello%20world', {
  with: { type: 'bytes' },
});
assert.strictEqual(Buffer.from(dataBytes.default).toString(), 'hello world');

const dataJsAsText = await import('data:text/javascript,export{}', {
  with: { type: 'text' },
});
assert.strictEqual(dataJsAsText.default, 'export{}');

const dataJsAsBytes = await import('data:text/javascript,export{}', {
  with: { type: 'bytes' },
});
assert.strictEqual(Buffer.from(dataJsAsBytes.default).toString(), 'export{}');

const dataInvalidUtf8 = await import('data:text/plain,%66%6f%80%6f', {
  with: { type: 'text' },
});
assert.strictEqual(dataInvalidUtf8.default, 'fo\ufffdo');

const jsAsText = await import('../fixtures/syntax/bad_syntax.js', {
  with: { type: 'text' },
});
assert.match(jsAsText.default, /^var foo bar;/);

const jsAsBytes = await import('../fixtures/syntax/bad_syntax.js', {
  with: { type: 'bytes' },
});
assert.match(Buffer.from(jsAsBytes.default).toString(), /^var foo bar;/);

const jsonAsText = await import('../fixtures/invalid.json', {
  with: { type: 'text' },
});
assert.match(jsonAsText.default, /"im broken"/);

await assert.rejects(
  import('data:text/plain,hello%20world'),
  { code: 'ERR_UNKNOWN_MODULE_FORMAT' },
);

await assert.rejects(
  import('../fixtures/file-to-read-without-bom.txt'),
  { code: 'ERR_UNKNOWN_FILE_EXTENSION' },
);
