// Flags: --no-experimental-import-text
import '../common/index.mjs';
import assert from 'node:assert';
import { registerHooks } from 'node:module';

// A user loader can use `text` with and without import attributes when the feature is disabled.

registerHooks({
  load(url, context, nextLoad) {
    if (url.endsWith('.txt')) {
      return nextLoad(url, { ...context, format: 'text' });
    }
    return nextLoad(url, context);
  },
});

const { default: text } = await import('../fixtures/file-to-read-without-bom.txt');
const { default: empty } = await import(
  '../fixtures/empty.txt',
  { with: { type: 'text' } }
);

assert.strictEqual(text, 'abc\ndef\nghi\n');
assert.strictEqual(empty, '');
