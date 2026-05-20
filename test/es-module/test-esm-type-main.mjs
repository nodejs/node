import { mustNotCall } from '../common/index.mjs';
import assert from 'assert';
import { importFixture } from '../fixtures/pkgexports.mjs';

(async () => {
  const m = await importFixture('type-main');
  assert.strictEqual(m.default, 'asdf');
})()
.catch(mustNotCall);
