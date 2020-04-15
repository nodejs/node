import { fileURLToPath } from 'url';
import { createRequire } from 'module';
import { strictEqual } from 'assert';

const require = createRequire(fileURLToPath(import.meta.url));

const production = process.env.NODE_ENV === 'production';
const expectValue = production ? 'production' : 'development';

strictEqual(require('pkgexports-dev'), expectValue);

(async () => {
  const { default: value } = await import('pkgexports-dev');
  strictEqual(value, expectValue);

  console.log(expectValue);
})();
