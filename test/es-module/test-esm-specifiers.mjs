// Flags: --experimental-modules --es-module-specifier-resolution=node
import { mustNotCall } from '../common/index.mjs';
import assert from 'assert';

// commonJS index.js
import commonjs from '../fixtures/es-module-specifiers/package-type-commonjs';
// esm index.js
import module from '../fixtures/es-module-specifiers/package-type-module';
// Notice the trailing slash
import success, { explicit, implicit, implicitModule, getImplicitCommonjs }
  from '../fixtures/es-module-specifiers/';

assert.strictEqual(commonjs, 'commonjs');
assert.strictEqual(module, 'module');
assert.strictEqual(success, 'success');
assert.strictEqual(explicit, 'esm');
assert.strictEqual(implicit, 'cjs');
assert.strictEqual(implicitModule, 'cjs');

async function main() {
  try {
    await import('../fixtures/es-module-specifiers/do-not-exist.js');
  } catch (e) {
    // Files that do not exist should throw
    assert.strictEqual(e.name, 'Error');
  }
  try {
    await getImplicitCommonjs();
  } catch (e) {
    // Legacy loader cannot resolve .mjs automatically from main
    assert.strictEqual(e.name, 'Error');
  }
}

main().catch(mustNotCall);
