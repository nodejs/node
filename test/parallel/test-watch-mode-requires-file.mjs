import '../common/index.mjs';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert';

const result = spawnSync(process.execPath, ['--watch'], { encoding: 'utf8' });
assert.strictEqual(result.status, 9);
assert.match(result.stderr, /--watch requires specifying a file/);
