import { run } from 'node:test';
import assert from 'node:assert';
import { tap } from 'node:test/reporters';

const stream = run({
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
