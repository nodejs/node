#!/usr/bin/env node

// Regenerates test/fixtures/yaml-test-suite.json from the data branch of
// https://github.com/yaml/yaml-test-suite.
//
// Usage: tools/update-yaml-test-suite.mjs [path-to-checkout]
//
// Without a path, the data branch is cloned into a temporary directory. The
// script prints the commit that the fixture was generated from, which goes
// into the header of test/parallel/test-yaml-test-suite.js.
//
// Every test of the suite is a directory named by its id, or a directory of
// numbered case directories. A test or case directory holds the name in "===",
// the input in "in.yaml", the expected documents in "in.json" (optional,
// several JSON values in a row) and an "error" file when the input is
// invalid. The fixture is an array of objects with the fields "id" ("<id>" or
// "<id>/<case>"), "name", "yaml", "error" and, when the suite provides it,
// "json".

import { execFileSync } from 'node:child_process';
import {
  existsSync,
  mkdtempSync,
  readdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const repository = 'https://github.com/yaml/yaml-test-suite';
const fixture = fileURLToPath(
  new URL('../test/fixtures/yaml-test-suite.json', import.meta.url));

let checkout = process.argv[2];
let temporary;
if (checkout === undefined) {
  temporary = mkdtempSync(join(tmpdir(), 'yaml-test-suite-'));
  checkout = join(temporary, 'data');
  execFileSync('git', ['clone', '--quiet', '--depth', '1', '--branch', 'data',
                       repository, checkout], { stdio: 'inherit' });
}
const commit = execFileSync('git', ['-C', checkout, 'rev-parse', 'HEAD'],
                            { encoding: 'utf8' }).trim();

// in.json holds one JSON value per document, one after the other.
function parseJSONStream(text) {
  const values = [];
  let pending = '';
  for (const line of text.split('\n')) {
    pending += `${line}\n`;
    if (pending.trim() === '') {
      pending = '';
      continue;
    }
    try {
      values.push(JSON.parse(pending));
      pending = '';
    } catch {
      // The value continues on the next line.
    }
  }
  if (pending.trim() !== '') {
    throw new Error(`Unterminated JSON value: ${pending}`);
  }
  return values;
}

function readCase(id, directory) {
  const test = {
    id,
    name: readFileSync(join(directory, '==='), 'utf8').trim(),
    yaml: readFileSync(join(directory, 'in.yaml'), 'utf8'),
    error: existsSync(join(directory, 'error')),
  };
  const json = join(directory, 'in.json');
  if (existsSync(json)) {
    test.json = parseJSONStream(readFileSync(json, 'utf8'));
  }
  return test;
}

const tests = [];
for (const entry of readdirSync(checkout, { withFileTypes: true })) {
  if (!entry.isDirectory() || !/^[0-9A-Z]{4}$/.test(entry.name)) continue;
  const directory = join(checkout, entry.name);
  if (existsSync(join(directory, 'in.yaml'))) {
    tests.push(readCase(entry.name, directory));
    continue;
  }
  for (const sub of readdirSync(directory, { withFileTypes: true })) {
    if (!sub.isDirectory()) continue;
    tests.push(readCase(`${entry.name}/${sub.name}`, join(directory, sub.name)));
  }
}
tests.sort((a, b) => (a.id < b.id ? -1 : 1));

writeFileSync(fixture, `${JSON.stringify(tests, null, 1)}\n`);
if (temporary !== undefined) rmSync(temporary, { recursive: true });

const errors = tests.filter((test) => test.error).length;
const withJSON = tests.filter((test) => test.json !== undefined).length;
console.log(`Wrote ${tests.length} tests (${errors} errors, ${withJSON} with ` +
            `expected JSON) from ${repository} commit ${commit}`);
