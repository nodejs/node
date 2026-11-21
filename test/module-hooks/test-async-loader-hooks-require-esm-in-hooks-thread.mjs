// Test that require(esm) of a module with imports works while loading a module.register() hooks module.
// Refs: https://github.com/nodejs/node/issues/66767
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--input-type=module',
    '--eval',
    `import { register } from 'node:module';
    register(${JSON.stringify(fixtures.fileURL('module-hooks/require-esm-in-hooks-thread/hooks.mjs'))});
    console.log('ok');`,
  ],
  {
    stdout: 'ok',
    stderr: '',
    trim: true,
  },
);
