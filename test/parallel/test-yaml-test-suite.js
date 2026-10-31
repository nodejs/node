// Flags: --expose-internals
'use strict';

// Runs the official YAML test suite (https://github.com/yaml/yaml-test-suite,
// data branch, commit 6ad3d2c62885d82fc349026c136ef560838fdf3d) against the
// parser. test/fixtures/yaml-test-suite.json holds, for every test, the input
// and either the expected documents as JSON or a flag that the input is an
// error.

require('../common');
const assert = require('node:assert');
const fixtures = require('../common/fixtures');
const { parseDocuments } = require('internal/yaml/parser');

// Tests the parser does not pass yet. A test that starts to pass must be
// removed from this list.
const knownFailures = new Set([
  // Duplicate keys, two empty keys and an alias of the first key: the parser
  // rejects them, which the suite does not check.
  '2JQS',
  'X38W',
]);

const tests = JSON.parse(fixtures.readSync('yaml-test-suite.json', 'utf8'));
assert.ok(tests.length > 300);

const failures = [];
let passed = 0;
for (const test of tests) {
  let failure;
  try {
    const documents = parseDocuments(test.yaml);
    if (test.error) {
      failure = `expected an error, parsed ${JSON.stringify(documents)}`;
    } else if (test.json !== undefined) {
      assert.deepStrictEqual(documents, test.json);
    }
  } catch (error) {
    if (!test.error) {
      failure = error.message;
    } else if (error.code !== 'ERR_INVALID_YAML') {
      failure = `unexpected ${error.name}: ${error.message}`;
    }
  }
  if (failure === undefined) {
    if (knownFailures.has(test.id)) {
      failures.push(`${test.id} (${test.name}) passes and must leave knownFailures`);
    } else {
      passed++;
    }
  } else if (!knownFailures.has(test.id)) {
    failures.push(`${test.id} (${test.name}): ${failure}`);
  }
}

assert.deepStrictEqual(failures, [], `${failures.length} failures:\n${failures.join('\n')}`);
assert.ok(passed > 0);
