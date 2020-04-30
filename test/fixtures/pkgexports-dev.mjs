import { fileURLToPath } from 'url';
import { createRequire } from 'module';
import { strictEqual, AssertionError } from 'assert';

const require = createRequire(fileURLToPath(import.meta.url));
const requireVal = require('pkgexports-dev');

(async () => {
  const { default: importVal } = await import('pkgexports-dev');
  strictEqual(requireVal, importVal);
  console.log(importVal);
})();
