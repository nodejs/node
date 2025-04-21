import '../common/index.mjs';
import assert from 'node:assert/strict';
import { Worker } from 'node:worker_threads';

function get_environment() {
  if (process.env.HAS_STARTED_WORKER) return 'in worker thread started by ES Module';
  return 'in ES Module';
}

assert.strictEqual(
  import.meta.main,
  true,
  `\`import.meta.main\` at top-level module ${get_environment()} should evaluate \`true\``
);

const { isMain: importedModuleIsMain } = await import('../fixtures/es-modules/import-meta-main.mjs');
assert.strictEqual(
  importedModuleIsMain,
  false,
  `\`import.meta.main\` at dynamically imported module ${get_environment()} should evaluate \`false\``
);

if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  new Worker(import.meta.filename);
}
