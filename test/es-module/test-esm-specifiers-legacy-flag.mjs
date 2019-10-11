// Flags: --es-module-specifier-resolution=node
import '../common/index.mjs';
import assert from 'assert';

// commonJS index.js
import commonjs from '../fixtures/es-module-specifiers/package-type-commonjs';
// esm index.js
import module from '../fixtures/es-module-specifiers/package-type-module';
// Notice the trailing slash
import success, { explicit, implicit, implicitModule }
  from '../fixtures/es-module-specifiers/';

assert.strictEqual(commonjs, 'commonjs');
assert.strictEqual(module, 'module');
assert.strictEqual(success, 'success');
assert.strictEqual(explicit, 'esm');
assert.strictEqual(implicit, 'cjs');
assert.strictEqual(implicitModule, 'cjs');
