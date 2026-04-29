#!/usr/bin/env node
// Writes doc/node-config-schema.json from generateConfigJsonSchema()
// in lib/internal/options.js.
//
// Usage:
//   node tools/gen_node_config_schema.mjs
//   node tools/gen_node_config_schema.mjs --check

import { spawnSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, relative } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const JSON_PATH = join(ROOT, 'doc/node-config-schema.json');

function getSchema() {
  const result = spawnSync(
    process.execPath,
    [
      '--expose-internals',
      '-p',
      'JSON.stringify(require("internal/options").generateConfigJsonSchema(), null, 2)',
    ],
    { encoding: 'utf8' },
  );
  if (result.status !== 0) {
    console.error(
      `Failed to read schema from option metadata:\n${result.stderr}`,
    );
    process.exit(1);
  }
  return `${result.stdout.trimEnd()}\n`;
}

const expected = getSchema();

if (process.argv.slice(2).includes('--check')) {
  if (readFileSync(JSON_PATH, 'utf8') !== expected) {
    console.error(
      `${relative(ROOT, JSON_PATH)} is out of date. ` +
      'Run `node tools/gen_node_config_schema.mjs` and commit the result.',
    );
    process.exit(1);
  }
  console.log(`${relative(ROOT, JSON_PATH)} is up to date.`);
} else {
  writeFileSync(JSON_PATH, expected);
  console.log(`Wrote ${relative(ROOT, JSON_PATH)}`);
}
