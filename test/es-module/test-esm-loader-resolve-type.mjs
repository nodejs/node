// Flags: --loader ./test/fixtures/es-module-loaders/hook-resolve-type.mjs
import { allowGlobals } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { strict as assert } from 'assert';
import * as fs from 'fs';

allowGlobals(global.getModuleTypeStats);

const basePath =
  new URL('./node_modules/', import.meta.url);

const rel = (file) => new URL(file, basePath);
const createDir = (path) => {
  if (!fs.existsSync(path)) {
    fs.mkdirSync(path);
  }
};

const moduleName = 'module-counter-by-type';

const moduleDir = rel(`${moduleName}`);
createDir(basePath);
createDir(moduleDir);
fs.cpSync(
  fixtures.path('es-modules', moduleName),
  moduleDir,
  { recursive: true }
);

const { importedESM: importedESMBefore,
        importedCJS: importedCJSBefore } = global.getModuleTypeStats();

await import(`${moduleName}`).finally(() => {
  fs.rmSync(basePath, { recursive: true, force: true });
});

const { importedESM: importedESMAfter,
        importedCJS: importedCJSAfter } = global.getModuleTypeStats();

// Dynamic import above should increment ESM counter but not CJS counter
assert.strictEqual(importedESMBefore + 1, importedESMAfter);
assert.strictEqual(importedCJSBefore, importedCJSAfter);
