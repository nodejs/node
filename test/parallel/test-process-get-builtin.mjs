import '../common/index.mjs';
import assert from 'node:assert';
import { builtinModules } from 'node:module';

for (const invalid of [1, undefined, null, false, [], {}, () => {}, Symbol('test')]) {
  assert.throws(() => process.getBuiltinModule(invalid), { code: 'ERR_INVALID_ARG_TYPE' });
}

for (const invalid of [
  'invalid', 'test', 'sea', 'test/reporter', 'internal/bootstrap/realm',
  'internal/deps/undici/undici', 'internal/util',
]) {
  assert.strictEqual(process.getBuiltinModule(invalid), undefined);
}

// Check that createRequire()(id) returns the same thing as process.getBuiltinModule(id).
const require = process.getBuiltinModule('module').createRequire(import.meta.url);
for (const id of builtinModules) {
  assert.strictEqual(process.getBuiltinModule(id), require(id));
}
// Check that import(id).default returns the same thing as process.getBuiltinModule(id).
for (const id of builtinModules) {
  const imported = await import(`node:${id}`);
  assert.strictEqual(process.getBuiltinModule(id), imported.default);
}

// builtinModules does not include 'test' which requires the node: prefix.
const ids = builtinModules.concat(['test']);
// Check that import(id).default returns the same thing as process.getBuiltinModule(id).
for (const id of ids) {
  const prefixed = `node:${id}`;
  const imported = await import(prefixed);
  assert.strictEqual(process.getBuiltinModule(prefixed), imported.default);
}
