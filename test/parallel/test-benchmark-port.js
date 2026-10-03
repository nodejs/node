'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const path = require('path');

const benchmarkers = path.resolve(__dirname, '../../benchmark/_http-benchmarkers.js');
const script = `console.log(require(${JSON.stringify(benchmarkers)}).PORT)`;

for (const [port, expected] of [
  [undefined, 12346],
  ['', 12346],
  ['0', 0],
  ['31337', 31337],
]) {
  const env = { ...process.env };
  delete env.PORT;
  if (port !== undefined)
    env.PORT = port;
  const child = spawnSync(process.execPath, ['-e', script], { env, encoding: 'utf8' });
  assert.ifError(child.error);
  assert.strictEqual(child.status, 0, child.stderr);
  assert.strictEqual(child.stdout, `${expected}\n`);
}
