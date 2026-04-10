// Flags: --experimental-import-text
import '../common/index.mjs';
import assert from 'assert';

import staticText from '../fixtures/file-to-read-without-bom.txt' with { type: 'text' };
import staticTextWithBOM from '../fixtures/file-to-read-with-bom.txt' with { type: 'text' };

const expectedText = 'abc\ndef\nghi\n';

assert.strictEqual(staticText, expectedText);
assert.strictEqual(staticTextWithBOM, expectedText);

const dynamicText = await import('../fixtures/file-to-read-without-bom.txt', {
  with: { type: 'text' },
});
assert.strictEqual(dynamicText.default, expectedText);

const dataText = await import('data:text/plain,hello%20world', {
  with: { type: 'text' },
});
assert.strictEqual(dataText.default, 'hello world');

const dataJsAsText = await import('data:text/javascript,export{}', {
  with: { type: 'text' },
});
assert.strictEqual(dataJsAsText.default, 'export{}');

const dataInvalidUtf8 = await import('data:text/plain,%66%6f%80%6f', {
  with: { type: 'text' },
});
assert.strictEqual(dataInvalidUtf8.default, 'fo\ufffdo');

const jsAsText = await import('../fixtures/syntax/bad_syntax.js', {
  with: { type: 'text' },
});
assert.match(jsAsText.default, /^var foo bar;/);

const jsonAsText = await import('../fixtures/invalid.json', {
  with: { type: 'text' },
});
assert.match(jsonAsText.default, /"im broken"/);

await assert.rejects(
  import('data:text/plain,hello%20world'),
  { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' },
);

await assert.rejects(
  import('../fixtures/file-to-read-without-bom.txt'),
  { code: 'ERR_UNKNOWN_FILE_EXTENSION' },
);
