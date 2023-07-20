import { run, test } from 'node:test';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert';
import { tap } from 'node:test/reporters';

const stream = run({
  files: [fileURLToPath(import.meta.url)],
  timeout: 1000
})
  .compose(tap)
  .pipe(process.stdout)

stream.on('test:fail', (f) => {
  assert.ok(false)
});
stream.on('test:pass', (p) => {
  assert.ok(false)
});

test('1 + 1 = 2', async () => {
  assert.strictEqual(1 + 1, 2, '1 + 1 = 2')
})
