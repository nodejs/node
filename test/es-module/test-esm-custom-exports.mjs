// Flags: --conditions=custom-condition -C another
import { mustCall } from '../common/index.mjs';
import { strictEqual } from 'assert';
import { requireFixture, importFixture } from '../fixtures/pkgexports.mjs';
[requireFixture, importFixture].forEach((loadFixture) => {
  loadFixture('pkgexports/condition')
    .then(mustCall((actual) => {
      strictEqual(actual.default, 'from custom condition');
    }));
});
