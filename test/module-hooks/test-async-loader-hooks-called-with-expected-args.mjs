// Test that loader hooks are called with all expected arguments
import '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/hooks-input.mjs'),
    fixtures.path('es-modules/json-modules.mjs'),
  ],
  {
    stdout(output) {
      const lines = output.split('\n');
      assert.match(lines[0], /{"url":"file:\/\/\/.*\/json-modules\.mjs","format":"test","shortCircuit":true}/);
      assert.match(lines[1], /{"source":{"type":"Buffer","data":\[.*\]},"format":"module","shortCircuit":true}/);
      assert.match(lines[2], /{"url":"file:\/\/\/.*\/experimental\.json","format":"test","shortCircuit":true}/);
      assert.match(lines[3], /{"source":{"type":"Buffer","data":\[.*\]},"format":"json","shortCircuit":true}/);
      assert.strictEqual(lines.length, 4);
    },
    stderr: '',
    trim: true,
  },
);
