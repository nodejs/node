'use strict';

const assert = require('assert');
const { spawnSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const tmpdir = require('./tmpdir');

const testScript = `
  const v8 = require('v8');
  const stats = v8.getHeapStatistics();
  const maxHeapSizeMB = Math.round(stats.heap_size_limit / 1024 / 1024);
  console.log(maxHeapSizeMB);
`;

tmpdir.refresh();
const scriptPath = path.join(tmpdir.path, 'heap-limit-test.js');
fs.writeFileSync(scriptPath, testScript);

const child = spawnSync(
  process.execPath,
  [scriptPath],
  {
    encoding: 'utf8',
    env: {
      ...process.env,
      NODE_OPTIONS: '--max-heap-size=750',
    },
  },
);

assert.strictEqual(
  child.status,
  0,
  [
    `Child process did not exit cleanly.`,
    `  Exit code: ${child.status}`,
    child.stderr ? `  Stderr: ${child.stderr.toString()}` : '',
    child.stdout ? `  Stdout: ${child.stdout.toString()}` : '',
  ].filter(Boolean).join('\n'),
);
const output = child.stdout.trim();
const heapLimit = Number(output);

assert.strictEqual(
  heapLimit,
  750,
  `max heap size is ${heapLimit}MB, expected 750MB`,
);
